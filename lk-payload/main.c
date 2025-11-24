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

static uint8_t microloader[0x400];
uint64_t g_boot, g_recovery, g_lk, g_misc;

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
    part_dev_t *dev = mt_part_get_device();
    
    fastboot_info("");
    fastboot_info("[amonet] Flashing LK payload...");
    
    if (sz > PAYLOAD_SIZE) {
        fastboot_fail("[amonet] Payload too large");
        return -1;
    }
    
    size_t ret = dev->write(dev, data, PAYLOAD_OFFSET, sz, BOOT0_PART);
    if (ret != sz) {
        fastboot_info("[amonet] Failed to write main payload");
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

    part_dev_t *dev = mt_part_get_device();
    dev->write(dev, bootloader_msg, g_misc * 0x200, 0x20, USER_PART);

    fastboot_okay("");
}

void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz)
{
    if (arg && strstr(arg, "recovery")) {
        write_boot_command("boot-recovery");
    }
    
    if (arg && strstr(arg, "bootloader")) {
        write_boot_command("boot-amonet");
    }
    
    cmd_reboot(arg, data, sz);
}

void cmd_flash_wrapper(const char *arg, void *data, unsigned sz)
{
    printf("cmd_flash_wrapper: arg=%s, sz=%u\n", arg, sz);
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
    else if (strstr(arg, "_amonet")) {
        char new_arg[32] = {0};
        strncpy(new_arg, arg, strstr(arg, "_amonet") - arg);
        cmd_flash(new_arg, data, sz);
        return;
    }

    cmd_flash(arg, data, sz);
}

int (*original_read)(part_dev_t *dev, uint64_t block_off, void *dst, size_t sz, int part) = (void*)(0x460533CD|1);

int read_func(part_dev_t *dev, uint64_t block_off, void *dst, size_t sz, int part) {
    printf("read_func hook\n");
    int ret = 0;
    if (block_off == g_boot * 0x200 || block_off == g_recovery * 0x200) {
        printf("demangle boot image - from 0x%08X\n", __builtin_return_address(0));

        if (sz < 0x400) {
            ret = original_read(dev, block_off + 0x400, dst, sz, part);
        } else {
            void *second_copy = (char*)dst + 0x400;
            ret = original_read(dev, block_off, dst, sz, part);
            memcpy(dst, second_copy, 0x400);
            memset(second_copy, 0, 0x400);
        }
    } else {
        ret = original_read(dev, block_off, dst, sz, part);
    }
    return ret;
}

static void parse_gpt(part_dev_t *dev) {
    uint8_t raw[0x800] = { 0 };
    dev->read(dev, 0x400, raw, sizeof(raw), USER_PART);
    
    for (int i = 0; i < sizeof(raw) / 0x80; ++i) {
        uint8_t *ptr = &raw[i * 0x80];
        uint8_t *name = ptr + 0x38;
        uint64_t start;
        memcpy(&start, ptr + 0x20, 8);
        
        if (start == 0) continue;
        
        if (strwcmp(name, "boot") == 0) {
            printf("found boot at 0x%08X\n", (uint32_t)start);
            g_boot = start;
        } else if (strwcmp(name, "recovery") == 0) {
            printf("found recovery at 0x%08X\n", (uint32_t)start);
            g_recovery = start;
        } else if (strwcmp(name, "lk") == 0) {
            printf("found lk at 0x%08X\n", (uint32_t)start);
            g_lk = start;
        } else if (strwcmp(name, "para") == 0) {
            printf("found para at 0x%08X\n", (uint32_t)start);
            g_misc = start;
        }
    }
}

void prepare_fastboot()
{
    uint16_t *patch;

    // Force fastboot no matter what
    patch = (uint16_t*)0x4602A3B8;
    patch[0] = 0x46c0; // nop
    patch[1] = 0x46c0; // nop

    // Disable built-in reboot command(s)
    patch = (void *)0x4602b190; // reboot-bootloader
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop

    patch = (void *)0x4602b180; // reboot
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop

    patch = (void *)0x4602b150; // flash
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop

    // Register custom command(s)
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("oem reboot-recovery", cmd_reboot_wrapper, 1);
    fastboot_register("flash:", cmd_flash_wrapper, 1);
}

int main() {
    int ret = 0, fastboot = 0;
    uint16_t *patch;
    uint32_t *patch32;

    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Updated version by k4y0z. Copyright 2019\n");
    printf("Ported to LG K10 by R0rt1z2. Copyright 2025\n");

    uint32_t **argptr = (void *)LK_BASE + 0x20;
    uint32_t *arg = *argptr;
    uint32_t *o_boot_mode = (uint32_t *)*argptr + 1; // argptr boot mode

    part_dev_t *dev = mt_part_get_device();
    parse_gpt(dev);

    printf("Backup microloader\n");
    ret = dev->read(dev, g_boot * 0x200, microloader, MICROLOADER_SIZE, USER_PART);
    if (ret != MICROLOADER_SIZE) {
        printf("failed to read microloader from boot partition\n");
    }

    // Restore the portion of LK that was overwritten by the microloader
    // this is way more than we actually need to restore, but it shouldn't hurt
    dev->read(dev, g_lk * 0x200 + 0x200, (char*)LK_BASE, 0x9A400, USER_PART); // +0x200 to skip lk header

    if (!g_boot || !g_recovery || !g_lk) {
        printf("failed to find boot, recovery or lk\n");
        printf("falling back to fastboot mode\n");
        fastboot = 1;
    }

    if (is_volume_down_pressed()) {
        printf("volume down pressed, entering fastboot mode\n");
        fastboot = 1;
    }

    if (is_volume_up_pressed()) {
        printf("volume up pressed, entering recovery mode\n");
        *g_boot_mode = 2;
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

        if (strncmp((char *)bootloader_msg, "boot-amonet", 11) == 0) {
            fastboot = 1;
            memset(bootloader_msg, 0, 0x10);
            dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
        }

        else if (strncmp((char *)bootloader_msg, "FASTBOOT_PLEASE", 15) == 0) {
            if (*g_boot_mode == 2) {
                memset(bootloader_msg, 0, 0x10);
                dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
            }
            else {
                fastboot = 1;
            }
        }

        else if (strncmp((char *)bootloader_msg, "boot-recovery", 13) == 0) {
            *g_boot_mode = 2;
            memset(bootloader_msg, 0, 0x10);
            dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
        }

        if (strncmp((char *)(bootloader_msg + 0x10), "UART_PLEASE", 11) == 0) {
            strcpy((char *)0x4bd45bd9, " printk.disable_uart=0");
        }
    }

    // hook up read function
    original_read = (void*)dev->read;
    dev->read = (void*)read_func;

    // mark the device as unlocked
    patch32 = (uint32_t*)0x46058138;
    patch32[0] = 0x2001; // movs r0, #1
    patch32[1] = 0x4770; // bx lr

    patch32 = (uint32_t*)0x46057e3c;
    patch32[0] = 0x2000; // movs r0, #0
    patch32[1] = 0x6020; // str r0, [r4, #0]  
    patch32[2] = 0x4770; // bx lr

    // mark secure download as disabled
    patch32 = (uint32_t*)0x460583FC;
    patch32[0] = 0x2000; // movs r0, #0
    patch32[1] = 0x4770; // bx lr

    // mark secure boot as disabled
    patch32 = (uint32_t*)0x460570BC;
    patch32[0] = 0x2000; // movs r0,
    patch32[1] = 0x4770; // bx lr

    // allow to flash any partition
    patch = (uint16_t*)0x4605815c;
    patch[0] = 0x2301; // movs r3, #1
    patch[1] = 0x600b; // str r3, [r1,#0] 
    patch[2] = 0x2000; // movs r0, #0
    patch[3] = 0x4770; // bx lr

    // allow to boot any partition
    patch32 = (uint32_t*)0x46026b48;
    patch32[0] = 0x2000; // movs r0, #0
    patch32[1] = 0x4770; // bx lr

    // lol
    strcpy((char*)0x4606f5b0, " - what is that?");
    // strcpy((char*)0x4606F5BC, " - what is that?");

    if (fastboot) {
	  *g_boot_mode = 99;
      prepare_fastboot();

      printf("Well since you're asking so nicely...\n");
      video_printf(" => HACKED FASTBOOT mode...\n");
    }

    // This is required because this LK was built with CFG_DTB_EARLY_LOADER_SUPPORT, which forces the
    // bootloader to attempt loading the DTB from the boot/recovery partition immediately after GPT
    // parsing. This fails because amonet crafts a malicious boot image with a heavily modified boot
    // header. Since DTB is required for kernel boot, we force the bootloader to load DTB after we
    // have hooked the read function to properly handle our crafted boot image structure.
    bldr_load_dtb((*g_boot_mode != 2) ? "boot" : "recovery");

    arch_clean_invalidate_cache_range(LK_BASE, LK_SIZE);

    printf("jump to lk at 0x%08X\n", (unsigned int)app);
    app();

    // Kill the thread, otherwise we'd waste CPU cycles spinning here and as a consequence have
    // horribly slow USB speeds in fastboot mode.
    thread_exit(0);
}