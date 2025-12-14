#pragma once

#define NULL ((void*)0)

struct device_t {
    uint32_t unk1;
    uint32_t unk2;
    uint32_t unk3;
    uint32_t unk4;
    size_t (*read)(struct device_t *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(struct device_t *dev, void *src, uint64_t block_off, size_t size, uint32_t part);
};

int (*is_key_pressed)(int key) = (void *)(0x4bd0f350|1);

struct device_t* (*get_device)() = (void*)(0x4bd1dee8|1);
void (*cache_clean)(void *addr, size_t sz) = (void*)0x4bd23f40;

void (*fastboot_info)(const char *reason) = (void *)(0x4bd26adc | 1);
void (*fastboot_fail)(const char *reason) = (void *)(0x4bd26b18 | 1);
void (*fastboot_okay)(const char *reason) = (void *)(0x4BD26CC0 | 1);

void (*cmd_flash)(const char *arg, void *data, unsigned sz) = (void *)(0x4bd28ed8 | 1);
void (*cmd_reboot)(const char *arg, void *data, unsigned sz) = (void *)(0x4bd274c8 | 1);

void (*fastboot_register)(const char *prefix, 
                          void (*handle)(const char *arg, void *data, unsigned sz), 
                          unsigned char security_enabled) = (void *)(0x4bd268ec | 1);

size_t (*video_printf)(const char *format, ...) = (void *)(0x4bd2a4d2|1);

uint32_t* g_boot_mode = (uint32_t*) 0x4bd5d364; // LK boot mode
uint32_t* i_boot_mode = (uint32_t*) 0x4bd6b1e4; // IDME boot mode

#define PAYLOAD_DST 0x41000000
#define PAYLOAD_SRC 0x80000
#define PAYLOAD_SIZE 0x80000

#define MICROLOADER_SRC (0x4BD5C000 - 0x50)
#define MICROLOADER_SIZE 0x400
#define MICROLOADER_BACKUP 0x44100000

#define LK_TEMP 0x44000000
#define LK_BASE 0x4BD00000
#define LK_SIZE (0x800 * 0x200)

#define KEY_PRIVACY 0x2F
#define KEY_VOLUP 0x25
#define KEY_VOLDOWN 0x24

#define BOOT0_PART 1
#define USER_PART 8

#define BACKUP_SRC 0x200000

#define CACHE_LINE 32

#define ICACHE 1
#define DCACHE 2
#define UCACHE (ICACHE | DCACHE)

#define MMSYS_BASE 0x14000000
#define MMSYS_SW0_B_RST (MMSYS_BASE + 0x140)