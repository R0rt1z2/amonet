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

int (*original_read)(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part) = (void*)0x4BD2AE2D;

uint64_t g_boot_a, g_boot_aa, g_boot_b, g_boot_bb, g_lk_a, g_lk_b, g_misc;

int read_func(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part) {
    printf("read_func hook\n");
    //hex_dump((void *)0x4BD00000, 0x100);
    //printf("block_off 0x%08X 0x%08X\n", block_off, *(&(block_off)+4));
    int ret = 0;
    if(block_off == g_boot_a * 0x200) {
      ret = original_read(dev, g_boot_aa * 0x200, dst, sz, part);
    } else if(block_off == (g_boot_a * 0x200) + 0x800) {
      ret = original_read(dev, (g_boot_aa * 0x200) + 0x800, dst, sz, part);
    } else if(block_off == g_boot_b * 0x200) {
      ret = original_read(dev, g_boot_bb * 0x200, dst, sz, part);
    } else if(block_off == (g_boot_b * 0x200) + 0x800) {
      ret = original_read(dev, (g_boot_bb * 0x200) + 0x800, dst, sz, part);
    } else {
      ret = original_read(dev, block_off, dst, sz, part);
    }
    return ret;
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
        if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00\x61\x00\x00\x00", 14) == 0) {
            printf("found boot_a at 0x%08X\n", start);
            g_boot_a = start;
        } else if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00\x61\x00\x61\x00\x00\x00", 16) == 0) {
            printf("found boot_aa at 0x%08X\n", start);
            g_boot_aa = start;
        } else if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00\x62\x00\x00\x00", 14) == 0) {
            printf("found boot_b at 0x%08X\n", start);
            g_boot_b = start;
        } else if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00\x62\x00\x62\x00\x00\x00", 16) == 0) {
            printf("found boot_bb at 0x%08X\n", start);
            g_boot_bb = start;
        } else if (memcmp(name, "l\x00k\x00_\x00\x61\x00\x00\x00", 10) == 0) {
            printf("found lk_a at 0x%08X\n", start);
            g_lk_a = start;
        } else if (memcmp(name, "l\x00k\x00_\x00\x62\x00\x00\x00", 10) == 0) {
            printf("found lk_b at 0x%08X\n", start);
            g_lk_b = start;
        } else if (memcmp(name, "m\x00i\x00s\x00\x63\x00\x00\x00", 10) == 0) {
            printf("found lk_b at 0x%08X\n", start);
            g_misc = start;
        }
    }
}

int main() {
    int ret = 0;
    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Ported to Echo 2 by k4y0z. Copyright 2019\n");

    int fastboot = 0;

    parse_gpt();

    if (!g_boot_aa || !g_boot_bb || !g_lk_a) {
        printf("failed to find boot, recovery or lk\n");
        printf("falling back to fastboot mode\n");
        fastboot = 1;
    }

    int (*app)() = (void*)0x4BD341D5;

    unsigned char overwritten[] = {
        0x6C, 0xBC, 0x05, 0x00, 0x60, 0xBC, 0x05, 0x00, 0x2D, 0xE9, 0xF8, 0x43, 0x5D, 0x48, 0x5E, 0x4D,
        0x78, 0x44, 0x5E, 0x4F, 0x39, 0xF0, 0x0E, 0xF8, 0x7D, 0x44, 0x29, 0x68, 0x7F, 0x44, 0x69, 0xBB,
        0x5B, 0x4C, 0x4F, 0xF4, 0x70, 0x52, 0x7C, 0x44, 0x20, 0x46, 0x3A, 0xF0, 0x0C, 0xE8, 0x20, 0x46,
        0x30, 0xF0, 0x00, 0xFE, 0x00, 0x28, 0x40, 0xF0, 0x9D, 0x80, 0x20, 0x46, 0x2C, 0x60, 0xFF, 0xF7,
    };

    memcpy((void*)0x4BD003C0, overwritten, sizeof(overwritten));
    //hex_dump((void*)0x4BD003C0, 0x100);

    uint8_t bootloader_msg[0x10] = { 0 };
    void *lk_dst = (void*)0x4BD00000;

    #define LK_SIZE (0x800 * 0x200)

    struct device_t *dev = get_device();

    if(g_misc) {
      // Read amonet-flag from MISC partition
      //dev->read(dev, g_misc * 0x200 + 0x4000, bootloader_msg, 0x10, USER_PART);
      dev->read(dev, g_misc * 0x200, bootloader_msg, 0x10, USER_PART);
      //video_printf("%s\n", bootloader_msg);
    }

    //uint8_t tmp[0x10] = { 0 };
    //dev->read(dev, g_boot_aa * 0x200, tmp, 0x10, USER_PART);
    uint8_t *tmp = (void*)0x45000020;
    if (strncmp(tmp, "FASTBOOT_PLEASE", 15) == 0) {
        fastboot = 1;
    }

    // flag on MISC
    else if(strncmp(bootloader_msg, "boot-amonet", 11) == 0) {
      fastboot = 1;
      // reset flag
      memset(bootloader_msg, 0, 11);
      dev->write(dev, bootloader_msg, g_misc * 0x200, 11, USER_PART);
    }

    // factory and factory advanced boot
    else if(*g_boot_mode == 4 || *g_boot_mode == 6){
        fastboot = 1;
    }

#ifdef RELOAD_LK
      printf("Disable interrupts\n");
      asm volatile ("cpsid if");
#endif

    uint16_t *patch;

    // force fastboot mode
    if (fastboot) {
        printf("well since you're asking so nicely...\n");

        patch = (void*)0x4BD34330;
        *patch = 0xE795;
    }

    // device is unlocked
    patch = (void*)0x4BD1D2FC;
    *patch++ = 0x2001; // movs r0, #1
    *patch = 0x4770;   // bx lr

    // This enables adb-root-shell
    // amzn_verify_limited_unlock (to set androidboot.unlocked_kernel=true)
    patch = (void*)0x4BD1D51C;
    *patch++ = 0x2000; // movs r0, #0
    *patch = 0x4770;   // bx lr

    /*
    // is_prod_device
    patch = (void*)0x4BD1DB0A;
    *patch = 0x2000; // movs r0, #0
    */

    //printf("(void*)dev->read 0x%08X\n", (void*)dev->read);
    //printf("(void*)&dev->read 0x%08X\n", (void*)&dev->read);

    // Force uart enable
    char* disable_uart = (char*)0x4BD4B0F8;
    strcpy(disable_uart, "printk.disable_uart=0");
    disable_uart = (char*)0x4BD4A56C;
    strcpy(disable_uart, " printk.disable_uart=0");

    uint32_t *patch32;

    if(!fastboot) {
      // hook bootimg read function

      original_read = (void*)dev->read;

      patch32 = (void*)0x4BD57670;
      *patch32 = (uint32_t)read_func;

      patch32 = (void*)&dev->read;
      *patch32 = (uint32_t)read_func;
    }

    patch32 = (void*)0x4BD641F4;
    *patch32 = 1; // // force 64-bit linux kernel

    printf("Clean lk\n");
    cache_clean(lk_dst, LK_SIZE);

#ifdef RELOAD_LK
    printf("About to jump to LK\n");

    uint32_t **argptr = (void*)0x4BD00020;
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
