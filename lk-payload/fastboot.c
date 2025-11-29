#include <fastboot.h>
#include <bootmode.h>
#include <debug.h>

extern uint64_t g_boot, g_recovery, g_lk, g_misc;
extern uint8_t microloader[MICROLOADER_SIZE];

static int inject_microloader(void *data, unsigned sz, const char *partition_name)
{
    uint8_t *image_data = (uint8_t *)data;
    
    fastboot_info("");
    fastboot_info("[amonet] Injecting microloader...");

    if (sz < 0x1000) {
        fastboot_info("[amonet] Image too small to inject microloader");
        return -1;
    }
    
    if (memcmp(image_data + 0x1000, ANDROID_MAGIC, ANDROID_MAGIC_SIZE) == 0) {
        fastboot_info("[amonet] Microloader already injected");
        return 0;
    }
    
    if (memcmp(image_data, ANDROID_MAGIC, ANDROID_MAGIC_SIZE) != 0) {
        fastboot_info("[amonet] Not a valid Android boot image");
        return -1;
    }
    
    memcpy(image_data + 0x1000, image_data, 0x1000);
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

void prepare_fastboot()
{
    uint16_t *patch;

    // force fastboot no matter what
    patch = (uint16_t*)0x4602A3B8;
    patch[0] = 0x46C0; // nop
    patch[1] = 0x46C0; // nop
    set_boot_mode(99);

    // disable built-in reboot command(s)
    patch = (void *)0x4602B190; // reboot-bootloader
    patch[0] = 0x46C0; // nop
    patch[1] = 0x46C0; // nop

    patch = (void *)0x4602B180; // reboot
    patch[0] = 0x46C0; // nop
    patch[1] = 0x46C0; // nop

    patch = (void *)0x4602B150; // flash
    patch[0] = 0x46C0; // nop
    patch[1] = 0x46C0; // nop

    // register our replacement command(s)
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("oem reboot-recovery", cmd_reboot_wrapper, 1);
    fastboot_register("flash:", cmd_flash_wrapper, 1);
}