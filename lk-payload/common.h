#pragma once

#include "libc.h"

typedef struct __part_dev part_dev_t;
typedef struct __part part_t;

struct __part_dev
{
    uint32_t init;
    uint32_t id;
    void *blkdev;
    void *init_dev;
    size_t (*read)(part_dev_t *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(part_dev_t *dev, void *src, uint64_t block_off, size_t size, uint32_t part);
};

struct __part
{
    uint32_t start_sect;
    uint32_t nr_sects;
    uint32_t part_attr;
    uint32_t part_id;
    const char *name;
    void *info;
};

int (*app)() = (void*)(0x4602a1c0|1);
void (*thread_exit)(int retcode) = (void(*)(int))(0x4601dbf8|1);
void (*video_printf)(const char *fmt, ...) = (void (*)(const char *fmt, ...))(0x46032DAC | 1);
void (*dprintf)(const char *fmt, ...) = (void (*)(const char* fmt, ...))(0x460333B8 | 1);
void (*arch_clean_invalidate_cache_range)(uint32_t start, uint32_t size) = (void (*)(uint32_t, uint32_t))0x4601c5cc;
part_dev_t *(*mt_part_get_device)() = (part_dev_t * (*)())(0x46053740 | 1);
int (*bldr_load_dtb)(char *boot_load_partition) = (int (*)(char *))(0x460276e8 | 1);
int (*is_volume_down_pressed)(void) = (int (*)(void))(0x46000160 | 1);
int (*is_volume_up_pressed)(void) = (int (*)(void))(0x46000150 | 1);

void (*fastboot_info)(const char *reason) = (void *)(0x4602adac | 1);
void (*fastboot_fail)(const char *reason) = (void *)(0x4602adf4 | 1);
void (*fastboot_okay)(const char *reason) = (void *)(0x4602afc0 | 1);

void (*cmd_flash)(const char *arg, void *data, unsigned sz) = (void *)(0x4602dcec | 1);
void (*cmd_reboot)(const char *arg, void *data, unsigned sz) = (void *)(0x4602b80c | 1);

void (*fastboot_register)(const char *prefix, 
                          void (*handle)(const char *arg, void *data, unsigned sz), 
                          unsigned char security_enabled) = (void *)(0x4602ab90 | 1);

uint32_t* g_boot_mode = (uint32_t*) 0x4609d120;

#define PAYLOAD_OFFSET 0x200000
#define PAYLOAD_ADDR 0x42000000
#define PAYLOAD_SIZE (16384*3)

#define BOOT0_PART 1
#define USER_PART 8

#define LK_BASE 0x46000000
#define LK_SIZE (0x800 * 0x200)

#define MICROLOADER_SRC (0x460038C4 - 0x50)
#define MICROLOADER_SIZE 0x1000

#define ANDROID_MAGIC "ANDROID!"
#define ANDROID_MAGIC_SIZE 8

#define BLOCK_SIZE 512