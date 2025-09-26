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

int (*original_read)(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part) = (void*)0x4BD2AE2D;
int (*app)() = (void*)0x4BD341D5;

uint64_t g_boot_a, g_boot_a_x, g_boot_b, g_boot_b_x, g_expdb, g_lk_a, g_lk_b, g_misc, g_recovery;
uint8_t boot_recovery = 0;

void set_led_ring(uint8_t colors[12][3]) {
    static uint8_t frame[36];
    for (int i = 0; i < 12; i++) {
        frame[i*3] = colors[i][0];
        frame[i*3+1] = colors[i][1]; 
        frame[i*3+2] = colors[i][2];
    }
    led_update(1, frame);
    led_write(0x25, 0); 
}

void* led_animation_thread(void* arg) {
    while (1) {
        for (int step = 0; step < 36; step++) {
            uint8_t frame[12][3];
            for (int i = 0; i < 12; i++) {
                int pos = step + i;
                while (pos >= 12) pos -= 12;
                if (pos < 2) { frame[i][0] = 0xFF; frame[i][1] = 0x00; frame[i][2] = 0x00; }
                else if (pos < 4) { frame[i][0] = 0xFF; frame[i][1] = 0x7F; frame[i][2] = 0x00; }
                else if (pos < 6) { frame[i][0] = 0x00; frame[i][1] = 0xFF; frame[i][2] = 0x00; }
                else if (pos < 8) { frame[i][0] = 0x00; frame[i][1] = 0xFF; frame[i][2] = 0xFF; }
                else if (pos < 10) { frame[i][0] = 0x00; frame[i][1] = 0x00; frame[i][2] = 0xFF; }
                else { frame[i][0] = 0xFF; frame[i][1] = 0x00; frame[i][2] = 0xFF; }
            }
            set_led_ring(frame);
            thread_sleep(50);
        }
    }
    return NULL;
}

void create_led_thread() {
  thread_t* led_thread = thread_create("rainbow", led_animation_thread, NULL, 10, 4096);
  if (led_thread) {
    thread_resume(led_thread);
  }
}

int read_func(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part) {
    printf("read_func hook\n");
    printf("block_off 0x%08X 0x%08X\n", block_off, *(&(block_off)+4));
    printf("dev 0x%08X dst 0x%08X sz 0x%08X part 0x%08X\n", dev, dst, sz, part);

    int ret = 0;

    if (block_off == g_boot_a * 0x200) {
      if (boot_recovery) {
        block_off = g_recovery * 0x200;
      }
      else {
        block_off = g_boot_a_x * 0x200;
      }
    } else if (block_off == (g_boot_a * 0x200) + 0x800) {
      if (boot_recovery) {
         block_off = (g_recovery * 0x200) + 0x800;
      }
      else {
        block_off = (g_boot_a_x * 0x200) + 0x800;
      }
    } else if (block_off == g_boot_b * 0x200) {
      if (boot_recovery) {
        block_off = g_recovery * 0x200;
      }
      else {
        block_off = g_boot_b_x * 0x200;
      }
    } else if (block_off == (g_boot_b * 0x200) + 0x800) {
      if (boot_recovery) {
        block_off = (g_recovery * 0x200) + 0x800;
      }
      else {
        block_off = (g_boot_b_x * 0x200) + 0x800;
      }
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
        if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00\x61\x00\x00\x00", 14) == 0) {
            printf("found boot_a at 0x%08X\n", start);
            g_boot_a = start;
        } else if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00\x61\x00_\x00x\x00\x00\x00", 18) == 0) {
            printf("found boot_a_x at 0x%08X\n", start);
            g_boot_a_x = start;
        } else if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00\x62\x00\x00\x00", 14) == 0) {
            printf("found boot_b at 0x%08X\n", start);
            g_boot_b = start;
        } else if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00\x62\x00_\x00x\x00\x00\x00", 18) == 0) {
            printf("found boot_b_x at 0x%08X\n", start);
            g_boot_b_x = start;
        } else if (memcmp(name, "l\x00k\x00_\x00\x61\x00\x00\x00", 10) == 0) {
            printf("found lk_a at 0x%08X\n", start);
            g_lk_a = start;
        } else if (memcmp(name, "l\x00k\x00_\x00\x62\x00\x00\x00", 10) == 0) {
            printf("found lk_b at 0x%08X\n", start);
            g_lk_b = start;
        } else if (memcmp(name, "m\x00i\x00s\x00\x63\x00\x00\x00", 10) == 0) {
            printf("found misc at 0x%08X\n", start);
            g_misc = start;
        } else if (memcmp(name, "r\x00\x65\x00\x63\x00o\x00v\x00\x65\x00r\x00y\x00\x00\x00", 18) == 0) {
            printf("found recovery at 0x%08X\n", start);
            g_recovery = start;
        } else if (memcmp(name, "\x65\x00\x78\x00\x70\x00\x64\x00\x62\x00\x00\x00", 12) == 0) {
            printf("found expdb at 0x%08X\n", start);
            g_expdb = start;
        }
    }
}

void mtk_wdt_reset(void) {
    volatile uint32_t *wdt_regs = (volatile uint32_t *)0x10007000;
    wdt_regs[6] = 0x1971;
    wdt_regs[0] = 0x22000014;
    wdt_regs[5] = 0x1209;
}

void (*fastboot_info)(const char *reason) = (void *)(0x4bd34814 | 1);
void (*fastboot_fail)(const char *reason) = (void *)(0x4bd3485c | 1);
void (*fastboot_okay)(const char *reason) = (void *)(0x4bd34a20 | 1);

void (*fastboot_register)(const char *prefix, 
                          void (*handle)(const char *arg, void *data, unsigned sz), 
                          unsigned char security_enabled) = (void *)(0x4bd345e4 | 1);

void (*cmd_flash)(const char *arg, void *data, unsigned sz) = (void *)(0x4bd36d68 | 1);

void cmd_flash_wrapper(const char *arg, void *data, unsigned sz) {
    const char *name = arg + 1;
    
    if (strncmp(name, "boot_a_amonet", 13) == 0) {
        printf("boot_a_amonet -> boot_a\n");
        cmd_flash("boot_a", data, sz);
        return;
    }

    if (strncmp(name, "boot_b_amonet", 13) == 0) {
        printf("boot_b_amonet -> boot_b\n");
        cmd_flash("boot_b", data, sz);
        return;
    }
    
    if (strncmp(name, "boot_a", 6) == 0) {
        printf("boot_a -> boot_a_x\n");
        cmd_flash("boot_a_x", data, sz);
        return;
    }

    if (strncmp(name, "boot_b", 6) == 0) {
        printf("boot_b -> boot_b_x\n");
        cmd_flash("boot_b_x", data, sz);
        return;
    }

    cmd_flash(name, data, sz);
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

void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz) {
    if (g_expdb) {
        fastboot_info("Rebooting into bootloader");

        uint8_t bootloader_msg[0x20] = { 0 };
        strcpy((char*)bootloader_msg, "boot-amonet");
        
        struct device_t *dev = get_device();
        dev->write(dev, bootloader_msg, g_expdb * 0x200, 0x20, USER_PART);
        
        fastboot_okay("");
        mtk_wdt_reset();
    } else {
        fastboot_fail("No expdb partition found!");
    }
}

void prepare_fastboot() {
    uint16_t *patch;
  
    // Disable built-in flash command
    patch = (void*)0x4BD34B68;
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop
    fastboot_register("flash", cmd_flash_wrapper, 1);

    // Disable built-in reboot bootloader command
    patch = (void*)0x4BD34BAE;
    *patch++ = 0x46C0; // nop
    *patch++ = 0x46C0; // nop
    fastboot_register("reboot-bootloader", cmd_reboot_bootloader, 1);

    // Rainbow LED
    patch = (void*)0x4BD349C8;
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop
    create_led_thread();

    // Add reboot recovery command
    fastboot_register("oem reboot-recovery", cmd_reboot_recovery, 1);
}

int main() {
    int ret = 0, fastboot = 0;
    uint16_t *patch;
    uint32_t *patch32;

    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("64-Bit version for biscuit by k4y0z and R0rt1z2. Copyright 2020-2025\n");

    parse_gpt();

    if (!g_boot_a_x || !g_boot_b_x || !g_lk_a) {
        printf("failed to find boot, recovery or lk\n");
        printf("falling back to fastboot mode\n");
        fastboot = 1;
    }

    unsigned char overwritten[] = {
        0x6C, 0xBC, 0x05, 0x00, 0x60, 0xBC, 0x05, 0x00, 0x2D, 0xE9, 0xF8, 0x43, 0x5D, 0x48, 0x5E, 0x4D,
        0x78, 0x44, 0x5E, 0x4F, 0x39, 0xF0, 0x0E, 0xF8, 0x7D, 0x44, 0x29, 0x68, 0x7F, 0x44, 0x69, 0xBB,
        0x5B, 0x4C, 0x4F, 0xF4, 0x70, 0x52, 0x7C, 0x44, 0x20, 0x46, 0x3A, 0xF0, 0x0C, 0xE8, 0x20, 0x46,
        0x30, 0xF0, 0x00, 0xFE, 0x00, 0x28, 0x40, 0xF0, 0x9D, 0x80, 0x20, 0x46, 0x2C, 0x60, 0xFF, 0xF7,
    };

    memcpy((void*)0x4BD003C0, overwritten, sizeof(overwritten));

    struct device_t *dev = get_device();

    // If action button is pressed, go to fastboot
    if (mtk_detect_key(KEY_UBER)) {
        printf("Action key pressed, going to fastboot\n");
        fastboot = 1;
    }

    // If mute button is pressed, go to recovery
    else if (detect_power_key()) {
        printf("Mute key pressed, booting recovery\n");
        *g_boot_mode = 2;
    }

    // factory and factory advanced boot
    if (*o_boot_mode == 4 ) {
      fastboot = 1;
    }

    // use advanced factory mode to boot recovery
    else if (*o_boot_mode == 6) {
      *g_boot_mode = 2;
    }

    if (g_misc) {
      uint8_t misc_msg[0x20] = { 0 };
      uint8_t expdb_msg[0x20] = { 0 };

      dev->read(dev, g_misc * 0x200, misc_msg, 0x10, USER_PART);
      dev->read(dev, g_expdb * 0x200, expdb_msg, 0x10, USER_PART);

      printf("Read msg from misc: %s\n", misc_msg);
      printf("Read msg from expdb: %s\n", expdb_msg);

      if (strncmp(misc_msg, "boot-recovery", 13) == 0) {
        *g_boot_mode = 2;
        memset(misc_msg, 0, 0x10);
        dev->write(dev, misc_msg, g_misc * 0x200, 0x10, USER_PART);
      }

      if (strncmp(expdb_msg, "boot-amonet", 11) == 0) {
        fastboot = 1;
        memset(expdb_msg, 0, 0x10);
        dev->write(dev, expdb_msg, g_expdb * 0x200, 0x10, USER_PART);
      }

      // This is because misc gets wiped on every boot?
      else if (strncmp(expdb_msg, "FASTBOOT_PLEASE", 15) == 0) {
        if (*g_boot_mode == 2) {
          memset(expdb_msg, 0, 0x10);
          dev->write(dev, expdb_msg, g_expdb * 0x200, 0x10, USER_PART);
        }
        else {
          fastboot = 1;
        }
      }

      if (strncmp(misc_msg + 0x10, "UART_PLEASE", 11) == 0) {
        char* disable_uart = (char*)0x4BD4B0F8;
        strcpy(disable_uart, "printk.disable_uart=0");
        disable_uart = (char*)0x4BD4A56C;
        strcpy(disable_uart, " printk.disable_uart=0");
      }
    }

    // Use seperate recovery partition
    if (*g_boot_mode == 2){
        if(g_recovery) {
          boot_recovery = 1;
          printf("Using recovery partition\n");
          // kernel checks this to decide whether to enable USB or not
          *g_boot_mode = 0; 
        }
    }

    // Force fastboot mode
    if (fastboot) {
        printf("well since you're asking so nicely...\n");
        *g_boot_mode = 99;
        prepare_fastboot();
    }

    // The device is unlocked
    patch = (void*)0x4BD1D2FC;
    *patch++ = 0x2001; // movs r0, #1
    *patch = 0x4770;   // bx lr

    // Amazon specific unlock patch
    patch = (void*)0x4BD1D51C;
    *patch++ = 0x2000; // movs r0, #0
    *patch = 0x4770;   // bx lr

    // Hook bootimg read function
    original_read = (void*)dev->read;
    patch32 = (void*)0x4BD57670;
    *patch32 = (uint32_t)read_func;

    patch32 = (void*)&dev->read;
    *patch32 = (uint32_t)read_func;

    // Force 64-bit kernel
    patch32 = (void*)0x4BD641F4;
    *patch32 = 1;

    // Accomodate the max download size
    patch32 = (void*)0x4BD34CE0;
    *patch32 = 0x0380F503; // ADD.W	R3, R3, #0x400000

    printf("Clean lk\n");
    cache_clean((void *)LK_BASE, LK_SIZE);

    app();

    while (1) {}
}
