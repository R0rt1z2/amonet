#include <common.h>
#include <debug.h>
#include <bootmode.h>
#include <fastboot.h>

uint8_t microloader[0x400];
uint64_t g_boot, g_recovery, g_lk, g_misc;

int (*original_read_ptr)(part_dev_t *dev, uint64_t block_off, void *dst, size_t sz, int part);

int read_func(part_dev_t *dev, uint64_t block_off, void *dst, size_t sz, int part) {    
    int ret = 0;

    printf("read_func hook: dev=0x%08X, block_off=0x%llX, dst=0x%08X, sz=0x%X, part=%d\n",
           (uint32_t)dev, block_off, (uint32_t)dst, (uint32_t)sz, part);
    if (block_off == g_boot * 0x200 || block_off == g_recovery * 0x200) {
        printf("demangle %s image - from 0x%08X\n", 
               (block_off == g_boot * 0x200) ? "boot" : "recovery",
               __builtin_return_address(0));
        if (sz < 0x400) {
            ret = original_read_ptr(dev, block_off + 0x400, dst, sz, part);
        } else {
            void *second_copy = (char*)dst + 0x400;
            ret = original_read_ptr(dev, block_off, dst, sz, part);
            memcpy(dst, second_copy, 0x400);
            memset(second_copy, 0, 0x400);
        }
    } else {
        ret = original_read_ptr(dev, block_off, dst, sz, part);
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
        
        // skip over invalid entries
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
        } else if (strwcmp(name, "para") == 0) { // equals misc on older devices
            printf("found para at 0x%08X\n", (uint32_t)start);
            g_misc = start;
        }
    }
}

int main() {
    int ret = 0, fastboot = 0;
    uint16_t *patch;
    uint32_t *patch32;

    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Updated version by k4y0z. Copyright 2019\n");
    printf("Ported to LG K10 by R0rt1z2. Copyright 2025\n");

    // read the boot mode from the Preloader boot argument
    uint32_t **argptr = (void *)LK_BASE + 0x20;
    uint32_t *arg = *argptr;
    uint32_t *o_boot_mode = (uint32_t *)*argptr + 1; // argptr boot mode

    // parse GPT
    part_dev_t *dev = mt_part_get_device();
    parse_gpt(dev);

    // backup microloader before restoring LK
    printf("Backup microloader\n");
    ret = dev->read(dev, g_boot * 0x200, microloader, MICROLOADER_SIZE, USER_PART);
    if (ret != MICROLOADER_SIZE) {
        printf("failed to read microloader from boot partition!\n");
    }

    // restore the portion of LK that was overwritten by the microloader
    // this is way more than we actually need to restore, but it shouldn't hurt
    dev->read(dev, g_lk * 0x200 + 0x200, (char*)LK_BASE, 0x9A400, USER_PART); // +0x200 to skip lk header

    // make sure we found boot, recovery and lk partitions
    if (!g_boot || !g_recovery || !g_lk) {
        printf("failed to find boot, recovery or lk\n");
        printf("falling back to fastboot mode\n");
        fastboot = 1;
    }

    // handle boot modes based on volume keys
    if (is_volume_down_pressed()) {
        printf("volume down pressed, entering fastboot mode\n");
        fastboot = 1;
    }

    if (is_volume_up_pressed()) {
        printf("volume up pressed, entering recovery mode\n");
        set_boot_mode(2);
    }

    // use factory mode to force fastboot
    if (*o_boot_mode == 4) {
        fastboot = 1;
    }

    // use advanced factory mode to force recovery
    else if (*o_boot_mode == 6) {
        set_boot_mode(2);
    }

    // read the bootloader message from misc
    if (g_misc) {
        uint8_t bootloader_msg[0x20] = {0};
        dev->read(dev, g_misc * 0x200, bootloader_msg, 0x20, USER_PART);
        printf("Read bootloader_msg: %s\n", bootloader_msg);

        if (strncmp((char *)bootloader_msg, "boot-amonet", 11) == 0) {
            fastboot = 1;
            memset(bootloader_msg, 0, 0x10);
            dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
        }

        // this is for when we're coming from bootrom-step.sh
        else if (strncmp((char *)bootloader_msg, "FASTBOOT_PLEASE", 15) == 0) {
            if (*g_boot_mode == 2) {
                memset(bootloader_msg, 0, 0x10);
                dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
            }
            else {
                fastboot = 1;
            }
        }

        // stock forces LAF instead?
        else if (strncmp((char *)bootloader_msg, "boot-recovery", 13) == 0) {
            set_boot_mode(2);
            memset(bootloader_msg, 0, 0x10);
            dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);
        }
    }

    // hook up read function
    original_read_ptr = (void*)dev->read;
    dev->read = (void*)read_func;

    // mark the device as unlocked
    patch32 = (uint32_t*)0x46058138;
    patch32[0] = 0x2001; // movs r0, #1
    patch32[1] = 0x4770; // bx lr

    patch32 = (uint32_t*)0x46057E3C;
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
    patch = (uint16_t*)0x4605815C;
    patch[0] = 0x2301; // movs r3, #1
    patch[1] = 0x600b; // str r3, [r1,#0] 
    patch[2] = 0x2000; // movs r0, #0
    patch[3] = 0x4770; // bx lr

    // allow to boot any partition
    patch32 = (uint32_t*)0x46026B48;
    patch32[0] = 0x2000; // movs r0, #0
    patch32[1] = 0x4770; // bx lr

    // not everything has to be serious
    strcpy((char*)0x4606F5B0, " - what is that?");

    // prepare fastboot mode, since stock LK doesn't natively support it
    if (fastboot) {
      prepare_fastboot();

      printf("Well since you're asking so nicely...\n");
      video_printf(" => HACKED FASTBOOT mode: (%d) - xyz, k4y0z, R0rt1z2\n", *o_boot_mode);
    }

    // this is required because LK tries to load DTB very early and hits
    // microloader instead of an actual boot image
    bldr_load_dtb((get_boot_mode() != 2) ? "boot" : "recovery");

    // clean cache so our patches are visible
    arch_clean_invalidate_cache_range(LK_BASE, LK_SIZE);

    // re-execute mt_boot_init, and let LK continue from there
    app();

    // kill the thread we were spawned in, otherwise we'd waste CPU cycles
    // spinning here and as a consequence have bad USB speeds in fastboot
    thread_exit(0);
}