#include <inttypes.h>

#include "libc.h"

#include "common.h"

void low_uart_put(int ch)
{
    volatile uint32_t *uart_reg0 = (volatile uint32_t *)0x11002014;
    volatile uint32_t *uart_reg1 = (volatile uint32_t *)0x11002000;

    while (!((*uart_reg0) & 0x20)) {}

    *uart_reg1 = ch;
}

void _putchar(char character)
{
    if (character == '\n')
        low_uart_put('\r');
    low_uart_put(character);
}

size_t (*original_read)(struct device_t *dev, uint64_t block_off, void *dst, uint32_t sz, uint32_t part);

uint64_t g_boot, g_recovery, g_lk, g_misc;

int read_func(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part)
{
    printf("read_func hook\n");
    int ret = 0;
    if (block_off == g_boot * 0x200 || block_off == g_recovery * 0x200) {
        printf("demangle boot image - from 0x%08X\n", __builtin_return_address(0));

        if (sz < 0x400) {
            ret = original_read(dev, block_off + 0x400, dst, sz, part);
        }
        else {
            void *second_copy = (char *)dst + 0x400;
            ret = original_read(dev, block_off, dst, sz, part);
            memcpy(dst, second_copy, 0x400);
            memset(second_copy, 0, 0x400);
        }
    }
    else {
        printf("normal read - from 0x%08X\n", __builtin_return_address(0));
        ret = original_read(dev, block_off, dst, sz, part);
    }
    return ret;
}

static void parse_gpt(struct device_t *dev)
{
    uint8_t raw[0x800] = {0};
    dev->read(dev, 0x400, raw, sizeof(raw), USER_PART);
    for (int i = 0; i < sizeof(raw) / 0x80; ++i) {
        uint8_t *ptr = &raw[i * 0x80];
        uint8_t *name = ptr + 0x38;
        uint32_t start;
        memcpy(&start, ptr + 0x20, 4);
        if (memcmp(name, "b\x00o\x00o\x00t\x00\x00\x00", 10) == 0) {
            printf("found boot at 0x%08X\n", start);
            g_boot = start;
        }
        else if (memcmp(name, "r\x00\x65\x00\x63\x00o\x00v\x00\x65\x00r\x00y\x00\x00\x00", 18) == 0) {
            printf("found recovery at 0x%08X\n", start);
            g_recovery = start;
        }
        else if (memcmp(name, "l\x00k\x00\x00\x00", 6) == 0) {
            printf("found lk at 0x%08X\n", start);
            g_lk = start;
        }
        else if (memcmp(name, "M\x00I\x00S\x00\x43\x00\x00\x00", 10) == 0) {
            printf("found misc at 0x%08X\n", start);
            g_misc = start;
        }
    }
}

void mtk_wdt_reset(void)
{
    volatile uint32_t *wdt_regs = (volatile uint32_t *)0x10007000;
    wdt_regs[6] = 0x1971;
    wdt_regs[0] = 0x22000014;
    wdt_regs[5] = 0x1209;
}

static uint8_t microloader[0x400];

static int inject_microloader(void *data, unsigned sz, const char *partition_name)
{
    uint8_t *image_data = (uint8_t *)data;
    
    fastboot_info("");
    fastboot_info("[amonet] Injecting microloader...");

    if (sz < 0x800) {
        fastboot_info("[amonet] Image too small to inject microloader");
        return -1;
    }
    
    if (memcmp(image_data + 0x400, ANDROID_MAGIC, ANDROID_MAGIC_SIZE) == 0) {
        fastboot_info("[amonet] Microloader already injected");
        return 0;
    }
    
    if (memcmp(image_data, ANDROID_MAGIC, ANDROID_MAGIC_SIZE) != 0) {
        fastboot_info("[amonet] Not a valid Android boot image");
        return -1;
    }
    
    memcpy(image_data + 0x400, image_data, 0x400);
    memcpy(image_data, microloader, MICROLOADER_SIZE);
    
    fastboot_info("[amonet] OK");
    return 0;
}

static int flash_payload(void *data, unsigned sz)
{
    struct device_t *dev = get_device();
    
    fastboot_info("");
    fastboot_info("[amonet] Flashing LK payload...");
    
    if (sz > PAYLOAD_SIZE) {
        fastboot_fail("[amonet] Payload too large");
        return -1;
    }
    
    size_t ret = dev->write(dev, data, PAYLOAD_SRC, sz, BOOT0_PART);
    if (ret != sz) {
        fastboot_info("[amonet] Failed to write main payload");
        return -1;
    }
    
    fastboot_info("[amonet] OK");
    
    fastboot_info("[amonet] Flashing backup payload...");
    ret = dev->write(dev, data, BACKUP_SRC, sz, BOOT0_PART);
    if (ret != sz) {
        fastboot_fail("[amonet] Failed to write backup payload");
        return -1;
    }
    
    fastboot_info("[amonet] OK");
    return 0;
}

static void write_boot_command(const char *command)
{
    if (!g_misc) {
        fastboot_fail("No misc partition found!");
        return;
    }
    
    uint8_t bootloader_msg[0x20] = {0};
    strcpy((char *)bootloader_msg, command);

    struct device_t *dev = get_device();
    dev->write(dev, bootloader_msg, g_misc * 0x200, 0x20, USER_PART);

    fastboot_okay("");
    mtk_wdt_reset();
}

void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz)
{
    if (arg && strstr(arg, "recovery")) {
        write_boot_command("boot-recovery");
        return;
    }
    
    if (arg && strstr(arg, "bootloader")) {
        write_boot_command("boot-amonet");
        return;
    }
    
    cmd_reboot(arg, data, sz);
}

void cmd_flash_wrapper(const char *arg, void *data, unsigned sz)
{
    if (strcmp(arg, "boot") == 0 || strcmp(arg, "recovery") == 0) {
        if (inject_microloader(data, sz, arg) < 0) {
            fastboot_fail("Failed to inject microloader");
            return;
        }
    }
    else if (strcmp(arg, "lkp") == 0) {
        if (flash_payload(data, sz) < 0) {
            fastboot_fail("Failed to flash LK payload");
        } else {
            fastboot_okay("");
        }
        return;
    }

    cmd_flash(arg, data, sz);
}

void prepare_fastboot()
{
    uint16_t *patch;
    // Disable built-in reboot command(s)
    patch = (void *)0x4bd26e54; // reboot-bootloader
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop

    patch = (void *)0x4bd26e42; // reboot
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop

    patch = (void *)0x4bd26e0a; // flash
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop

    // Register custom command(s)
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("oem reboot-recovery", cmd_reboot_wrapper, 1);
    fastboot_register("flash:", cmd_flash_wrapper, 1);
}

int main()
{
    int ret = 0, fastboot = 0;
    uint16_t *patch;
    uint32_t *patch32;

    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Updated version by k4y0z. Copyright 2019\n");
    printf("Ported to checkers by R0rt1z2. Copyright 2025\n");

    uint32_t **argptr = (void *)0x4BD00020;
    uint32_t *arg = *argptr;
    uint32_t *o_boot_mode = (uint32_t *)*argptr + 1; // argptr boot mode

    arg[0x53] = 4; // force 64-bit linux kernel

    struct device_t *dev = get_device();
    parse_gpt(dev);

    if (!g_boot || !g_recovery || !g_lk)
    {
        printf("failed to find boot, recovery or lk\n");
        printf("falling back to fastboot mode\n");
        fastboot = 1;
    }

    printf("Backup microloader\n");
    memcpy(microloader, (void *)MICROLOADER_SRC, MICROLOADER_SIZE);

    unsigned char overwritten[80] = {
        0xF5, 0x0A, 0xD0, 0x4B, 0x91, 0x0E, 0xD0, 0x4B, 0x01, 0x09, 0xD0, 0x4B, 0x41, 0x0B, 0xD0, 0x4B,
        0xB1, 0x0C, 0xD0, 0x4B, 0x00, 0x84, 0xD5, 0x4B, 0x09, 0x0A, 0xD0, 0x4B, 0x79, 0x0A, 0xD0, 0x4B,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x1B, 0xD0, 0x4B,
        0x15, 0x1D, 0xD0, 0x4B, 0xC5, 0x1A, 0xD0, 0x4B, 0xB1, 0x1D, 0xD0, 0x4B, 0x35, 0x1A, 0xD0, 0x4B,
        0x09, 0x1C, 0xD0, 0x4B, 0xC1, 0x19, 0xD0, 0x4B, 0x9D, 0x1C, 0xD0, 0x4B, 0x00, 0x00, 0x00, 0x00
    };
    memcpy((void *)0x4BD5C000, overwritten, sizeof(overwritten));

    // Check if backup payload is present and copy if not
    dev->read(dev, BACKUP_SRC, (void *)0x45000000, PAYLOAD_SIZE, BOOT0_PART); // boot0 partition, read 512K
    if (memcmp((void *)PAYLOAD_DST, (void *)0x45000000, PAYLOAD_SIZE)) {
        printf("Backup payload not found...\n");
        printf("...copy payload to backup location\n");
        dev->write(dev, (void *)PAYLOAD_DST, BACKUP_SRC, PAYLOAD_SIZE, BOOT0_PART);
    }

    // Use factory mode to force fastboot
    if (*o_boot_mode == 4) {
        fastboot = 1;
    }

    // Use advanced factory mode to force recovery
    else if (*o_boot_mode == 6) {
        *g_boot_mode = 2;
    }

    if (g_misc) {
        uint8_t bootloader_msg[0x20] = {0};
        dev->read(dev, g_misc * 0x200, bootloader_msg, 0x20, USER_PART);
        printf("Read bootloader_msg: %s\n", bootloader_msg);

        if (strncmp(bootloader_msg, "boot-amonet", 11) == 0) {
            fastboot = 1;
            memset(bootloader_msg, 0, 0x10);
            dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
        }

        else if (strncmp(bootloader_msg, "FASTBOOT_PLEASE", 15) == 0) {
            if (*g_boot_mode == 2) {
                memset(bootloader_msg, 0, 0x10);
                dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
            }
            else {
                fastboot = 1;
            }
        }

        else if (strncmp(bootloader_msg, "boot-recovery", 13) == 0) {
            *g_boot_mode = 2;
            memset(bootloader_msg, 0, 0x10);
            dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
        }
    }

    char *disable_uart = (char *)0x4bd45bd9;
    strcpy(disable_uart, " printk.disable_uart=0");

    if (is_key_pressed(KEY_PRIVACY) && !recovery_keys()) {
        printf("Privacy key pressed, entering fastboot...\n");
        fastboot = 1;
    }

    if (fastboot) {
        printf("Well since you're asking so nicely...\n");
        prepare_fastboot();

        video_printf(" => HACKED FASTBOOT mode: (%d) - xyz, k4y0z, R0rt1z2\n", *o_boot_mode);
        *g_boot_mode = 99;

        // Make sure we don't trigger recovery mode
        patch = (void *)0x4bd0da88;
        *patch++ = 0x2000; // movs r0, #0
        *patch = 0x4770;   // bx lr
    }

    else if (*g_boot_mode == 2) {
        patch = (void *)0x4bd26520;
        *patch++ = 0x46C0; // nop
        *patch = 0x46C0;   // nop

        patch = (void *)0x4bd26528;
        *patch++ = 0x46C0; // nop
        *patch = 0x46C0;   // nop

        video_clean();
        video_printf(" => RECOVERY mode...\n");
    }

    // Enable all commands
    patch = (void *)0x4bd0d854;
    *patch++ = 0x2000; // movs r0, #0
    *patch = 0x4770;   // bx lr

    // Device is unlocked
    patch = (void *)0x4bd01ea0;
    *patch++ = 0x2001; // movs r0, #1
    *patch = 0x4770;   // bx lr

    // Hook bootimg read function
    original_read = (void *)dev->read;
    patch32 = (void *)&dev->read;
    *patch32 = (uint32_t)read_func;

    // This is so it looks consistent
    strcpy((char *)0x4bd46204, " => RECOVERY mode...\n");

    // This device seems to use a 32 bit kernel
    // patch32 = (void*)0x4BD681B8;
    // *patch32 = 1; // force 64-bit linux kernel

    printf("Clean lk\n");
    cache_clean((void *)LK_BASE, LK_SIZE);

    int (*app)() = (void *)(0x4bd263e0 | 1);
    app();

    while (1) {}
}
