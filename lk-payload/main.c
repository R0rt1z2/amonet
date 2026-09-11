#include "libc.h"
#include "debug.h"
#include "common.h"
#include "bootimg.h"

#include <bcbtool/lib/bcblib.h>
#include <idmelib.h>

int (*original_read)(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part) = (void*)0x4BD2AE2D;
int (*app)() = (void*)0x4BD341D5;
void (*lk_jump64)(uint32_t addr, uint32_t arg1, uint32_t arg2, uint32_t arg3) = (void*)(0x4BD3309C | 1);

uint64_t g_boot_a, g_boot_a_x, g_boot_b, g_boot_b_x, g_expdb, g_lk_a, g_lk_b, g_misc, g_recovery;
uint8_t boot_recovery = 0;

struct uboot_params {
    uint64_t device_type_id;
    uint64_t boot_argument;
};

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
  thread_t* led_thread = thread_create("rainbow", led_animation_thread, NULL, 1, 4096);
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

int is_64bit_kernel(uint8_t * bootopt_str)
{
    int i = 0;
    
    for (; i < (BOOT_ARGS_SIZE-0x16); i++) {
        if (0 == strncmp(&bootopt_str[i], "bootopt=", sizeof("bootopt=")-1)) {
            if (0 == strncmp(&bootopt_str[i+0x12], "64", sizeof("64")-1)) {
                return 1;
            }
            if (0 == strncmp(&bootopt_str[i+0x12], "32", sizeof("32")-1)) {
                return 0;
            }
        }
    }
    
    printf("Warning! No bootopt info found!\n");
    return 0;
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

static void print_bcb(struct bcb *data)
{
	int active_slot = bcblib_bcb_get_active_slot(data, false, false);
	printf("BCB [Magic: %.3s | Ver: %d]\n", data->magic, data->version);
	printf("Slot A: prio=%-2d tries=%-1d success=%d\n", 
		   data->slot[0].priority, data->slot[0].tries, data->slot[0].success);
	printf("Slot B: prio=%-2d tries=%-1d success=%d\n", 
		   data->slot[1].priority, data->slot[1].tries, data->slot[1].success);
	if (active_slot >= 0)
		printf("Active slot: %c\n", 'a' + active_slot);
	else
		printf("Active slot: NONE; WILL FAIL TO BOOT!\n");
}

static bool read_bcb(struct device_t *dev, struct bcb *data) {
    if (!g_misc)
        return false;

    uint32_t sector = (g_misc * 0x200) + (BCB_OFFSET & ~0x1FF);
    uint32_t offset = BCB_OFFSET & 0x1FF;
    uint8_t buf[0x200];

    if (dev->read(dev, sector, buf, sizeof(buf), USER_PART) != sizeof(buf)) {
        printf("Failed to read BCB\n");
        return false;
    }

    memcpy(data, buf + offset, sizeof(struct bcb));
    return true;
}

static bool write_bcb(struct device_t *dev, struct bcb *data) {
    if (!g_misc)
        return false;

    uint32_t sector = (g_misc * 0x200) + (BCB_OFFSET & ~0x1FF);
    uint32_t offset = BCB_OFFSET & 0x1FF;
    uint8_t buf[0x200];

    if (dev->read(dev, sector, buf, sizeof(buf), USER_PART) != sizeof(buf))
        return false;

    memcpy(buf + offset, data, sizeof(struct bcb));

    if (dev->write(dev, buf, sector, sizeof(buf), USER_PART) != sizeof(buf)) {
        printf("Failed to write BCB\n");
        return false;
    }

    return true;
}

static int idme_get_device_type_id(char *out, size_t out_size) {
    static uint8_t idme_buf[IDME_SIZE] __attribute__((aligned(64)));
    struct device_t *dev = get_device();

    if (dev->read(dev, 0, idme_buf, sizeof(idme_buf), BOOT1_PART) != sizeof(idme_buf)) {
        printf("Failed to read IDME\n");
        return -1;
    }

    struct idme *hdr = (struct idme *)idme_buf;
    if (!idmelib_magic_valid(hdr)) {
        printf("IDME magic invalid\n");
        return -1;
    }

    return idmelib_get_var(hdr, "device_type_id", out, out_size);
}

static uint8_t get_chainload(void) {
    struct device_t *dev = get_device();
    uint8_t buf[0x10] = { 0 };

    dev->read(dev, (g_expdb + CHAINLOAD_FLAG_BLOCK) * 0x200, buf, sizeof(buf), USER_PART);
    return buf[0] == 1;
}

static void set_chainload(uint8_t enabled) {
    struct device_t *dev = get_device();
    uint8_t buf[0x10] = { 0 };

    buf[0] = enabled ? 1 : 0;
    dev->write(dev, buf, (g_expdb + CHAINLOAD_FLAG_BLOCK) * 0x200, sizeof(buf), USER_PART);
}

static int flash_uboot(void *data, unsigned sz)
{
    struct device_t *dev = get_device();
    size_t aligned = (sz + 0x1FF) & ~0x1FF;

    fastboot_info("");
    fastboot_info("[amonet] Flashing u-boot...");

    if (aligned > sz)
        memset((uint8_t *)data + sz, 0, aligned - sz);

    if (dev->write(dev, data, (g_expdb + UBOOT_BLOCK) * 0x200, aligned, USER_PART) != aligned) {
        fastboot_fail("Failed to write u-boot");
        return -1;
    }

    fastboot_info("[amonet] OK");
    return 0;
}

static int flash_payload(void *data, unsigned sz)
{
    struct device_t *dev = get_device();

    fastboot_info("");
    fastboot_info("[amonet] Flashing LK payload...");

    if (((uint8_t *)data)[0] == 0x00) {
        size_t aligned = (sz + 0x1FF) & ~0x1FF;
        if (aligned > sz)
            memset((uint8_t *)data + sz, 0, aligned - sz);

        for (int slot = 0; slot < 2; slot++) {
            uint64_t base = ((slot == 0 ? g_boot_a : g_boot_b) + PAYLOAD_BLOCK) * 0x200;
            if (dev->write(dev, data, base, aligned, USER_PART) != aligned) {
                fastboot_fail("Failed to write boot.payload");
                return -1;
            }
        }
    } else {
        uint8_t prefix[0x400];

        if (sz > 0x3000) {
            fastboot_fail("Payload too large");
            return -1;
        }

        for (int slot = 0; slot < 2; slot++) {
            uint64_t base = ((slot == 0 ? g_boot_a : g_boot_b) + PAYLOAD_BLOCK) * 0x200;

            if (dev->read(dev, base, prefix, sizeof(prefix), USER_PART) != sizeof(prefix)) {
                fastboot_fail("Failed to read boot.payload header");
                return -1;
            }

            size_t first_chunk = sizeof(prefix) - 0x240;
            if (first_chunk > sz)
                first_chunk = sz;
            memcpy(prefix + 0x240, data, first_chunk);

            if (dev->write(dev, prefix, base, sizeof(prefix), USER_PART) != sizeof(prefix)) {
                fastboot_fail("Failed to write boot.payload header");
                return -1;
            }

            if (sz > first_chunk) {
                uint8_t *rest = (uint8_t *)data + first_chunk;
                size_t rest_sz = sz - first_chunk;
                size_t aligned = (rest_sz + 0x1FF) & ~0x1FF;

                if (aligned > rest_sz)
                    memset(rest + rest_sz, 0, aligned - rest_sz);

                if (dev->write(dev, rest, base + sizeof(prefix), aligned, USER_PART) != aligned) {
                    fastboot_fail("Failed to write payload body");
                    return -1;
                }
            }
        }
    }

    fastboot_info("[amonet] OK");
    return 0;
}

void mtk_wdt_reset(void) {
    volatile uint32_t *wdt_regs = (volatile uint32_t *)0x10007000;
    wdt_regs[6] = 0x1971;
    wdt_regs[0] = 0x22000014;
    wdt_regs[5] = 0x1209;
}

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

    if (strncmp(name, "lkp", 3) == 0) {
        if (flash_payload(data, sz) < 0) {
            fastboot_fail("Failed to flash LK payload");
        } else {
            fastboot_okay("");
        }
        return;
    }

    if (strncmp(name, "uboot", 5) == 0) {
        if (flash_uboot(data, sz) < 0) {
            fastboot_fail("Failed to flash u-boot");
        } else {
            fastboot_okay("");
        }
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

void cmd_set_active(const char *arg, void *data, unsigned sz) {
    const char *slot = arg + 1;

    if (*slot != 'a' && *slot != 'b') {
        fastboot_fail("Invalid slot. Use 'a' or 'b'");
        return;
    }

    struct device_t *dev = get_device();
    struct bcb bcb_data;

    if (!read_bcb(dev, &bcb_data)) {
        fastboot_fail("Failed to read BCB");
        return;
    }

    if (!bcblib_bcb_magic_valid(&bcb_data))
        bcblib_bcb_init(&bcb_data);

    int idx = *slot - 'a';
    bcb_data.slot[idx] = BCB_SLOT_METADATA_ACTIVE;
    bcb_data.slot[1 - idx] = BCB_SLOT_METADATA_EMPTY;

    if (!write_bcb(dev, &bcb_data)) {
        fastboot_fail("Failed to write BCB");
        return;
    }

    char msg[32];
    npf_snprintf(msg, sizeof(msg), "Active slot set to: %c", *slot);
    fastboot_okay("");
    fastboot_info(msg);
    fastboot_okay("");
}

void cmd_chainload(const char *arg, void *data, unsigned sz) {
    if (!g_expdb) {
        fastboot_fail("No expdb partition found!");
        return;
    }

    const char *value = arg + 1;

    if (*arg == '\0') {
        fastboot_info(get_chainload() ? "Chainload is enabled" : "Chainload is disabled");
        fastboot_okay("");
        return;
    }

    if (*value == '0') {
        set_chainload(0);
        fastboot_info("Chainload disabled");
        fastboot_okay("");
    } else if (*value == '1') {
        set_chainload(1);
        fastboot_info("Chainload enabled");
        fastboot_okay("");
    } else {
        fastboot_fail("Invalid value. Use 0 or 1");
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

    // Control chainloading
    fastboot_register("oem chainload", cmd_chainload, 1);

    // This is so we can easily switch slots
    fastboot_publish("slot-count", "2");
    fastboot_register("set_active", cmd_set_active, 1);

    // This is so we can easily identify the amonet version
    fastboot_publish("amonet-version", AMONET_VERSION);

    // Expose the current chainload status
    fastboot_publish("chainload", get_chainload() ? "1" : "0");
}

static char current_slot[2] = "a";

int main() {
    int ret = 0, fastboot = 0;
    uint16_t *patch;
    uint32_t *patch32;

    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("64-Bit version for radar by k4y0z and R0rt1z2. Copyright 2020-2026\n");
    printf("Version: %s, built on %s at %s\n", AMONET_VERSION, __DATE__, __TIME__);

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

    struct bcb bcb_data;
    if (read_bcb(dev, &bcb_data)) {
        bool dirty = false;

        if (!bcblib_bcb_magic_valid(&bcb_data)) {
            bcblib_bcb_init(&bcb_data);
            dirty = true;
        } else {
            print_bcb(&bcb_data);
        }

        // If both slots are marked as failed, we'll probably brick.
        // To prevent that, we basically set success to whatever the
        // current slot is (in this case, the one with highest prio).
        if (!bcblib_metadata_get_success(&bcb_data.slot[0]) &&
            !bcblib_metadata_get_success(&bcb_data.slot[1])) {
            int current = bcblib_bcb_get_active_slot(&bcb_data, false, false);

            // Unlikely to happen since we called bcblib_bcb_init()?
            if (current < 0)
                current = 0;

            bcblib_metadata_set_success(&bcb_data.slot[current], true);
            dirty = true;
        }

        if (dirty) {
            printf("Saved you from bricking your device\n");
            write_bcb(dev, &bcb_data);
            print_bcb(&bcb_data);
        }

        int active = bcblib_bcb_get_active_slot(&bcb_data, false, false);
        if (active >= 0)
            current_slot[0] = 'a' + active;
    }

    fastboot_publish("current-slot", current_slot);

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
    }

    // Unconditionally enable UART
    char* disable_uart = (char*)0x4BD4B0F8;
    strcpy(disable_uart, "printk.disable_uart=0");
    disable_uart = (char*)0x4BD4A56C;
    strcpy(disable_uart, " printk.disable_uart=0");

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
    } else {
        // This is so we can boot both 64 & 32 bit kernels
        uint8_t dst[0x800] = {0};
        uint64_t g_boot = (current_slot[0] == 'a') ? g_boot_a_x : g_boot_b_x;
        dev->read(dev, (boot_recovery ? g_recovery : g_boot) * 0x200, dst, sizeof(dst), USER_PART);

        boot_img_hdr *hdr = (boot_img_hdr *)dst;
        if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE) == 0) {
            if (is_64bit_kernel(hdr->cmdline)) {
                printf("64-bit kernel detected, forcing 64-bit mode\n");
                uint32_t *patch32 = (void *)0x4BD641F4;
                *patch32 = 1;
            } else { // .. assume 32 bits
                printf("32-bit kernel detected, forcing 32-bit mode\n");
                uint32_t *patch32 = (void *)0x4BD641F4;
                *patch32 = 0;
            }
        }
    }

    if (!fastboot && !boot_recovery && get_chainload()) {
        static char device_type_id[64] = { 0 };
        static struct uboot_params params;

        printf("Chainload enabled, jumping to u-boot\n");

        idme_get_device_type_id(device_type_id, sizeof(device_type_id));
        printf("device_type_id: %s\n", device_type_id);

        params.device_type_id = (uint32_t)device_type_id;
        params.boot_argument = G_BOOT_ARG;

        printf("params @ 0x%08X device_type_id=0x%08X boot_argument=0x%08X\n",
               (uint32_t)&params, (uint32_t)params.device_type_id, (uint32_t)params.boot_argument);

        dev->read(dev, (g_expdb + UBOOT_BLOCK) * 0x200, (void *)UBOOT_ADDR, UBOOT_SIZE, USER_PART);
        cache_clean((void *)UBOOT_ADDR, UBOOT_SIZE);

        lk_jump64(UBOOT_ADDR, (uint32_t)&params, 0, 1);
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

    // Accomodate the max download size
    patch32 = (void*)0x4BD34CE0;
    *patch32 = 0x0380F503; // ADD.W	R3, R3, #0x400000

    printf("Clean lk\n");
    cache_clean((void *)LK_BASE, LK_SIZE);

    app();

    // Kill the thread we were spawned in, otherwise we'd waste CPU cycles
    // spinning here and as a consequence have bad USB speeds in fastboot
    thread_exit(0);
}