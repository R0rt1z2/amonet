#include <inttypes.h>

#include "libc.h"

#include "common.h"

//#define RELOAD_LK

void low_uart_put(int ch) {
    volatile uint32_t *uart_reg0 = (volatile uint32_t*)0x11002014;
    volatile uint32_t *uart_reg1 = (volatile uint32_t*)0x11002000;

    while ( !((*uart_reg0) & 0x20) )
    {}

    *uart_reg1 = ch;
}

void _putchar(char character)
{
    if (character == '\n')
        low_uart_put('\r');
    low_uart_put(character);
}

int (*original_read)(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part) = (void*)0x4BD35539;
int (*app)() = (void*)0x4BD3ED61;

uint64_t g_boot, g_boot_x, g_lk, g_misc, g_recovery, g_recovery_x;

int read_func(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part) {
    printf("read_func hook\n");
    //hex_dump((void *)0x4BD003C0, 0x100);

    printf("block_off 0x%08X 0x%08X\n", block_off, *(&(block_off)+4));
    printf("dev 0x%08X dst 0x%08X sz 0x%08X part 0x%08X\n", dev, dst, sz, part);
    int ret = 0;
    if(block_off == g_boot * 0x200) {
      block_off = g_boot_x * 0x200;
    } else if(block_off == (g_boot * 0x200) + 0x800) {
      block_off = (g_boot_x * 0x200) + 0x800;
    } else if(block_off == g_recovery * 0x200) {
      block_off = g_recovery_x * 0x200;
    } else if(block_off == (g_recovery * 0x200) + 0x800) {
      block_off = (g_recovery_x * 0x200) + 0x800;
    }
    return original_read(dev, block_off, dst, sz, part);
}

static void parse_gpt() {
    uint8_t raw[0x1000] = { 0 };
    struct device_t *dev = get_device();
    dev->read(dev, 0x400, raw, sizeof(raw), USER_PART);
    for (int i = 0; i < sizeof(raw) / 0x80; ++i) {
        uint8_t *ptr = &raw[i * 0x80];
        uint8_t *name = ptr + 0x38;
        uint32_t start;
        memcpy(&start, ptr + 0x20, 4);
        if (memcmp(name, "b\x00o\x00o\x00t\x00\x00\x00", 10) == 0) {
            printf("found boot at 0x%08X\n", start);
            g_boot = start;
        } else if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00x\x00\x00\x00", 14) == 0) {
            printf("found boot_x at 0x%08X\n", start);
            g_boot_x = start;
        } else if (memcmp(name, "l\x00k\x00\x00\x00", 6) == 0) {
            printf("found lk at 0x%08X\n", start);
            g_lk = start;
        } else if (memcmp(name, "M\x00I\x00S\x00\x43\x00\x00\x00", 10) == 0) {
            printf("found misc at 0x%08X\n", start);
            g_misc = start;
        } else if (memcmp(name, "r\x00\x65\x00\x63\x00o\x00v\x00\x65\x00r\x00y\x00\x00\x00", 18) == 0) {
            printf("found recovery at 0x%08X\n", start);
            g_recovery = start;
        } else if (memcmp(name, "r\x00\x65\x00\x63\x00o\x00v\x00\x65\x00r\x00y\x00_\x00x\x00\x00\x00", 22) == 0) {
            printf("found recovery_x at 0x%08X\n", start);
            g_recovery_x = start;
        }
    }
}

int main() {
    int ret = 0;
    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Original 64-bit version by k4y0z. Copyright 2019\n");
    printf("Ported to giza by R0rt1z2. Copyright 2021\n");

    int fastboot = 0;

    parse_gpt();

    if (!g_boot_x || !g_recovery_x || !g_lk) {
        printf("failed to find boot, recovery or lk\n");
        printf("falling back to fastboot mode\n");
        fastboot = 1;
    }

    unsigned char overwritten[] = {
	      0x6C, 0xBC, 0x07, 0x00, 0x60 ,0xBC, 0x07, 0x00
    };
    memcpy((void*)0x4BD003C0, overwritten, sizeof(overwritten));

    uint32_t **argptr = (void*)0x4BD00020;
    //printf("(void*)*argptr 0x%08X\n", (void*)*argptr);
    //printf("(void*)argptr 0x%08X\n", (void*)argptr);
    *argptr = (void*)0x4be5e208;
    //hex_dump((void*)*argptr, 0x180);

    uint8_t bootloader_msg[0x20] = { 0 };

    void *lk_dst = (void*)0x4BD00000;

    #define LK_SIZE (0x800 * 0x200)

    struct device_t *dev = get_device();

    // factory and factory advanced boot
    if(*o_boot_mode == 4 ) {
      fastboot = 1;
    }

    // use advanced factory mode to boot recovery
    else if(*o_boot_mode == 6) {
      *g_boot_mode = 2;
    }

    else if(g_misc) {
      // Read amonet-flag from MISC partition
      dev->read(dev, g_misc * 0x200, bootloader_msg, 0x20, USER_PART);
      //dev->read(dev, g_misc * 0x200 + 0x4000, bootloader_msg, 0x10, USER_PART);
      printf("bootloader_msg: %s\n", bootloader_msg);

      // temp flag on MISC
      if(strncmp(bootloader_msg, "boot-amonet", 11) == 0) {
        fastboot = 1;
        // reset flag
        memset(bootloader_msg, 0, 0x10);
        dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
      }

      // perm flag on MISC
      else if(strncmp(bootloader_msg, "FASTBOOT_PLEASE", 15) == 0) {
        // only reset flag in recovery-boot
        if(*g_boot_mode == 2) {
          memset(bootloader_msg, 0, 0x10);
          dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
        }
        else {
          fastboot = 1;
        }
      }

      // recovery flag on MISC
      else if(strncmp(bootloader_msg, "boot-recovery", 13) == 0) {
        *g_boot_mode = 2;
        // reset flag
        memset(bootloader_msg, 0, 0x10);
        dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
      }

      // UART flag on MISC
      if(strncmp(bootloader_msg + 0x10, "UART_PLEASE", 11) == 0) {
        // Force uart enable
        char* disable_uart = (char*)0x4BD65E44;
        strcpy(disable_uart, " printk.disable_uart=0");
	      char* disable_uart2 = (char*)0x4BD66AC0;
        strcpy(disable_uart, "printk.disable_uart=0");
      }
    }

#ifdef RELOAD_LK
      printf("Disable interrupts\n");
      asm volatile ("cpsid if");
#endif

    uint16_t *patch;

    // force fastboot mode
    if (fastboot) {
        printf("well since you're asking so nicely...\n");
	
	if(*g_boot_mode == 2) *o_boot_mode = 2;

	*g_boot_mode = 99;

        video_printf("=> HACKED FASTBOOT mode: (%d) - xyz, k4y0z, R0rt1z2\n", *o_boot_mode);
    }
    else if(*g_boot_mode == 2) {
      video_printf("=> RECOVERY mode...");
    }

    printf("g_boot_mode %u\n", *g_boot_mode);
    printf("i_boot_mode %u\n", *i_boot_mode);
    printf("o_boot_mode %u\n", *o_boot_mode);

    // device is unlocked
    patch = (void*)0x4BD2054C;
    *patch++ = 0x2001; // movs r0, #1
    *patch = 0x4770;   // bx lr

    // amzn_verify_limited_unlock (to set androidboot.unlocked_kernel=true)
    // patch = (void*)0x4BD2080C;
    // *patch++ = 0x2000; // movs r0, #0
    // *patch = 0x4770;   // bx lr

    //printf("(void*)dev->read 0x%08X\n", (void*)dev->read);
    //printf("(void*)&dev->read 0x%08X\n", (void*)&dev->read);

    uint32_t *patch32;

    // hook bootimg read function

    original_read = (void*)dev->read;

    patch32 = (void*)0x4BD76188;
    *patch32 = (uint32_t)read_func;

    patch32 = (void*)&dev->read;
    *patch32 = (uint32_t)read_func;

    // patch max-download-size to accommodate for payload
    patch32 = (void*)0x4BD3F802;
    *patch32 = 0x0380F503; // ADD.W	R3, R3, #0x400000

    printf("Clean lk\n");
    cache_clean(lk_dst, LK_SIZE);

#ifdef RELOAD_LK
    printf("About to jump to LK\n");
    
    uint32_t *arg = *argptr;
    arg[0x53] = 4; // force 64-bit linux kernel

    asm volatile (
        "mov r4, %0\n" 
        "mov r3, %1\n"
        "blx r3\n"
        : : "r" (arg), "r" (lk_dst) : "r3", "r4");

    printf("Failure\n");
#else
    app();
#endif

    while (1) {

    }
}
