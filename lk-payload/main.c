#include <inttypes.h>

#include "libc.h"
#include "common.h"


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

int (*original_read)(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part) = (void*)(0x4BD2FFF9|1);
int (*app)() = (void*)(0x4BD39864|1);

void (*fastboot_info)(const char *reason) = (void *)(0x4bd3a00c | 1);
void (*fastboot_fail)(const char *reason) = (void *)(0x4bd3a054 | 1);
void (*fastboot_okay)(const char *reason) = (void *)(0x4bd3a204 | 1);

void (*fastboot_register)(const char *prefix, 
                          void (*handle)(const char *arg, void *data, unsigned sz), 
                          unsigned char security_enabled) = (void *)(0x4bd39ddc | 1);

uint64_t g_boot, g_boot_x, g_lk, g_misc, g_recovery, g_recovery_x;

int read_func(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part) {
    printf("read_func hook\n");
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

static void parse_gpt(struct device_t *dev) {
    uint8_t raw[0x1000] = { 0 };
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

void mtk_wdt_reset(void) {
    volatile uint32_t *wdt_regs = (volatile uint32_t *)0x10007000;
    wdt_regs[6] = 0x1971;
    wdt_regs[0] = 0x22000014;
    wdt_regs[5] = 0x1209;
}

void cmd_reboot_recovery(const char *arg, void *data, unsigned sz) {
    if (g_misc) {
        fastboot_info("Rebooting into recovery");

        uint8_t bootloader_msg[0x20] = { 0 };
        strcpy((char*)bootloader_msg, "boot-recovery");
        
        struct device_t *dev = get_device();
        dev->write(dev, bootloader_msg, g_misc * 0x200, 0x20, USER_PART);
        
        fastboot_okay("");
        mtk_wdt_reset();
    } else {
        fastboot_fail("No misc partition found!");
    }
}

void register_fastboot_commands() {
    fastboot_register("oem reboot-recovery", cmd_reboot_recovery, 1);
}

int main() {
    int ret = 0, fastboot = 0;

    printf("This is LK-payload by xyz and k4y0z. Copyright 2019\n");
    printf("Ported to Echo Spot (rook) by R0rt1z2. Copyright 2025\n");

    struct device_t *dev = get_device();
    parse_gpt(dev);

    if (!g_boot_x || !g_recovery_x || !g_lk) {
        printf("Failed to find required partitions in GPT!\n");
        printf("Falling back to fastboot mode...\n");
        fastboot = 1;
    }

    // Restore the 0x4BD00000-0x4BD50000 range, a part of it was overwritten
    // this is way more than we actually need to restore, but it shouldn't hurt
    dev->read(dev, g_lk * 0x200 + 0x200, (char*)LK_BASE, 0x50000, USER_PART); // +0x200 to skip lk header

    // Restore the boot argument pointer
    uint32_t **argptr = (void*)0x4BD00020;
    *argptr = (void*)0x4BE5E208;

    // Use factory mode to force fastboot
    if(*o_boot_mode == 4 ) {
      fastboot = 1;
    }

    // Use advanced factory mode to force recovery
    else if(*o_boot_mode == 6) {
      *g_boot_mode = 2;
    }

    if (g_misc) {
      uint8_t bootloader_msg[0x20] = { 0 };
      dev->read(dev, g_misc * 0x200, bootloader_msg, 0x20, USER_PART);
      printf("Read bootloader_msg: %s\n", bootloader_msg);

      if(strncmp(bootloader_msg, "boot-amonet", 11) == 0) {
        fastboot = 1;
        memset(bootloader_msg, 0, 0x10);
        dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
      }

      else if(strncmp(bootloader_msg, "FASTBOOT_PLEASE", 15) == 0) {
        if (*g_boot_mode == 2) {
          memset(bootloader_msg, 0, 0x10);
          dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
        }
        else {
          fastboot = 1;
        }
      }

      else if(strncmp(bootloader_msg, "boot-recovery", 13) == 0) {
        *g_boot_mode = 2;
        memset(bootloader_msg, 0, 0x10);
        dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
      }

      if (strncmp(bootloader_msg + 0x10, "UART_PLEASE", 11) == 0) {
        char* disable_uart = (char*)0x4BD5EBB8;
        strcpy(disable_uart, " printk.disable_uart=0");
        char* disable_uart2 = (char*)0x4BD5F88C;
        strcpy(disable_uart, "printk.disable_uart=0");
      }
    }

    if (fastboot) {
      printf("Well since you're asking so nicely...\n");
	    *g_boot_mode = 99;
      video_printf("=> HACKED FASTBOOT mode (%d)...\n", *g_boot_mode);
      register_fastboot_commands();
    }

    if (*g_boot_mode == 2) {
      video_printf("=> RECOVERY mode...");
    }

    // The device is unlocked
    uint16_t *patch;
    patch = (void*)0x4BD1D2FC;
    *patch++ = 0x2001; // movs r0, #1
    *patch = 0x4770;   // bx lr

    // Amazon specific unlock patch
    patch = (void*)0x4BD1D51C;
    *patch++ = 0x2000; // movs r0, #0
    *patch = 0x4770;   // bx lr

    original_read = (void*)dev->read;

    uint32_t *patch32;
    patch32 = (void*)0x4BD70D04;
    *patch32 = (uint32_t)read_func;

    patch32 = (void*)&dev->read;
    *patch32 = (uint32_t)read_func;

    // Accomodate the max download size
    patch32 = (void*)0x4BD3A4FA;
    *patch32 = 0x0380F503; // ADD.W	R3, R3, #0x400000

    cache_clean((void*)0x4BD00000, LK_SIZE);
    app();

    while (1) {}
}
