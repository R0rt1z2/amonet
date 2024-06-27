#include <inttypes.h>

#include "libc.h"

#include "common.h"

//#define RELOAD_LK

void low_uart_put(int ch) {
    volatile uint32_t *uart_reg0 = (volatile uint32_t*)0x11009014;
    volatile uint32_t *uart_reg1 = (volatile uint32_t*)0x11009000;

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

void hex_dump(const void* data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*)data)[i]);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i+1) % 8 == 0 || i+1 == size) {
            printf(" ");
            if ((i+1) % 16 == 0) {
                printf("\n");
                // printf("|  %s \n", ascii);
            } else if (i+1 == size) {
                ascii[(i+1) % 16] = '\0';
                if ((i+1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                // printf("|  %s \n", ascii);
                printf("\n");
            }
        }
    }
}

int (*original_read)(part_dev_t *dev, uint64_t dev_addr, void *dst, uint32_t size) = (void*)(0x81e0a2c8|1);
int (*app)() = (void*)(0x81e3c640|1);

uint64_t g_boot, g_boot_x, g_lk, g_misc, g_recovery, g_recovery_x;

int read_func(part_dev_t *dev, uint64_t block_off, void *dst, uint32_t sz) {
    printf("read_func hook\n");
    printf("block_off 0x%08X 0x%08X\n", block_off, *(&(block_off)+4));
    printf("dev 0x%08X dst 0x%08X sz 0x%08X\n", dev, dst, sz);
    int ret = 0;

    if (block_off == (g_boot * 0x200) + 0x1000000) {
        block_off = (g_boot_x * 0x200) + 0x1000000;
    } else if (block_off == ((g_boot * 0x200) + 0x800) + 0x1000000) {
        block_off = ((g_boot_x * 0x200) + 0x800) + 0x1000000;
    } else if (block_off == (g_recovery * 0x200) + 0x1000000) {
        block_off = (g_recovery_x * 0x200) + 0x1000000;
    } else if (block_off == ((g_recovery * 0x200) + 0x800) + 0x1000000) {
        block_off = ((g_recovery_x * 0x200) + 0x800) + 0x1000000;
    }

    return original_read(dev, block_off, dst, sz);
}

static void parse_gpt() {
    uint8_t raw[0x1000] = { 0 };
    mmc_read(0x0, (uint32_t *)raw, sizeof(raw));

    for (int i = 0; i < sizeof(raw) / 0x80; ++i) {
        uint8_t *ptr = &raw[i * 0x80];
        uint8_t *name = ptr + 0x38;
        uint32_t start;
        memcpy(&start, ptr + 0x20, 4);
        if (memcmp(name, "b\x00o\x00o\x00t\x00\x00\x00", 10) == 0) {
            g_boot = start;
            printf("found boot at 0x%08X\n", start);
        } else if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00x\x00\x00\x00", 14) == 0) {
            g_boot_x = start;
            printf("found boot_x at 0x%08X\n", start);
        } else if (memcmp(name, "U\x00\x42\x00O\x00O\x00T\x00\x00\x00", 12) == 0) {
            g_lk = start ;
            printf("found lk at 0x%08X\n", start);
        } else if (memcmp(name, "M\x00I\x00S\x00\x43\x00\x00\x00", 10) == 0) {
            g_misc = start;
            printf("found misc at 0x%08X\n", start);
        } else if (memcmp(name, "r\x00\x65\x00\x63\x00o\x00v\x00\x65\x00r\x00y\x00\x00\x00", 18) == 0) {
            g_recovery = start;
            printf("found recovery at 0x%08X\n", start);
        } else if (memcmp(name, "r\x00\x65\x00\x63\x00o\x00v\x00\x65\x00r\x00y\x00_\x00x\x00\x00\x00", 22) == 0) {
            g_recovery_x = start;
            printf("found recovery_x at 0x%08X\n", start);
        }
    }
}

__attribute__((section(".text.start"))) int main() {
    int ret = 0;

    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Original version for sloane by k4y0z and t0x1cSH. Copyright 2020\n");
    printf("Ported to ariel ariel by R0rt1z2. Copyright 2024\n");

    // We need to clean the cache first, since we jumped straight to the payload
    cache_clean((void *)PAYLOAD_DST, PAYLOAD_SIZE);

    int fastboot = 0;

    parse_gpt();

    if (!g_boot_x || !g_recovery_x || !g_lk) {
        printf("failed to find boot, recovery or lk\n");
        printf("falling back to fastboot mode\n");
        fastboot = 1;
    }

    uint8_t bootloader_msg[0x20] = { 0 };

    part_dev_t *dev = get_device();

    // Restore argptr
    uint32_t **argptr = (void*)0x81e00020;

    printf("g_boot_mode %u\n", *g_boot_mode);
    printf("f_boot_mode %u\n", *f_boot_mode);

    // factory and factory advanced boot
    if(*g_boot_mode == 4 ) {
      fastboot = 1;
    }

    // use advanced factory mode to boot recovery
    else if(*g_boot_mode == 6) {
      *g_boot_mode = 2;
    }

    else if(g_misc) {
      // Read amonet-flag from MISC partition
      //dev->read(dev, g_misc * 0x200, bootloader_msg, 0x20);
      //dev->read(dev, g_misc * 0x200 + 0x4000, bootloader_msg, 0x10, USER_PART);
      //printf("bootloader_msg: %s\n", bootloader_msg);

      // temp flag on MISC
      if(strncmp(bootloader_msg, "boot-amonet", 11) == 0) {
        fastboot = 1;
        // reset flag
        memset(bootloader_msg, 0, 0x10);
        //dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10);
      }

      // perm flag on MISC
      else if(strncmp(bootloader_msg, "FASTBOOT_PLEASE", 15) == 0) {
        // only reset flag in recovery-boot
        if(*g_boot_mode == 2) {
          memset(bootloader_msg, 0, 0x10);
          //dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10);
        }
        else {
          fastboot = 1;
        }
      }

      // UART flag on MISC
      if(strncmp(bootloader_msg + 0x10, "UART_PLEASE", 11) == 0) {
        // Force uart enable
        char* disable_uart = (char*)0x81e60934;
        strcpy(disable_uart, " printk.disable_uart=0");
        char* disable_uart2 = (char*)0x81e60fd0;
        strcpy(disable_uart2, "printk.disable_uart=0");
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
        video_printf("=> HACKED FASTBOOT mode: (%d) - xyz, k4y0z, t0x1cSH, R0rt1z2\n", *g_boot_mode);
	    *g_boot_mode = 99;
    }
    else if(*g_boot_mode == 2) {
      video_printf("=> RECOVERY mode...");
    }

    // device is unlocked
    uint8_t **unlocked = (uint8_t**)0x81e800b4;
    (*unlocked)[0x16] = 0x1;

    // printf("(void*)dev->read 0x%08X\n", (void*)dev->read);
    // printf("(void*)&dev->read 0x%08X\n", (void*)&dev->read);

    uint32_t *patch32;

    // hook bootimg read function
    original_read = (void*)dev->read;

    patch32 = (void*)0x81E6C7C0;
    *patch32 = (uint32_t)read_func;

    patch32 = (void*)&dev->read;
    *patch32 = (uint32_t)read_func;

    printf("Clean lk\n");
    cache_clean((void *)LK_BASE, LK_SIZE);

#ifdef RELOAD_LK
    printf("About to jump to LK\n");

    asm volatile (
        "mov r4, %0\n" 
        "mov r3, %1\n"
        "blx r3\n"
        : : "r" (arg), "r" (LK_BASE) : "r3", "r4");

    printf("Failure\n");
#else
    app();
#endif

    while (1) {

    }
}
