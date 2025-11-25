#pragma once

#include "libc.h"

typedef struct part_dev {
    uint32_t init;
    uint32_t id;
    void *blkdev;
    void *init_dev;
    size_t (*read)(struct part_dev *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(struct part_dev *dev, void *src, uint64_t block_off, size_t size, uint32_t part);
} part_dev_t;

#define PAYLOAD_OFFSET 0x200000
#define PAYLOAD_ADDR   0x42000000
#define PAYLOAD_SIZE   (16384*3)

#define BOOT0_PART 1
#define USER_PART  8

#define BLOCK_SIZE 512

#define LK_BASE 0x46000000
#define LK_SIZE (0x800 * 0x200)

#define MICROLOADER_SRC (0x460038C4 - 0x50)
#define MICROLOADER_SIZE 0x1000

#define ANDROID_MAGIC "ANDROID!"
#define ANDROID_MAGIC_SIZE 8

#define app ((int (*)(void))(0x4602A1C0|1))
#define thread_exit ((void (*)(int))(0x4601DBF8|1))
#define dprintf ((void (*)(const char*, ...))(0x460333B8|1))
#define arch_clean_invalidate_cache_range ((void (*)(uint32_t, uint32_t))(0x4601C5CC))
#define mt_part_get_device ((part_dev_t* (*)(void))(0x46053740|1))
#define original_read ((int (*)(part_dev_t*, uint64_t, void*, size_t, int))(0x460533CD|1))
#define bldr_load_dtb ((int (*)(char*))(0x460276E8|1))
#define is_volume_down_pressed ((int (*)(void))(0x46000160|1))
#define is_volume_up_pressed ((int (*)(void))(0x46000150|1))
#define fastboot_info ((void (*)(const char*))(0x4602ADAC|1))
#define fastboot_fail ((void (*)(const char*))(0x4602ADF4|1))
#define fastboot_okay ((void (*)(const char*))(0x4602AFC0|1))
#define cmd_flash ((void (*)(const char*, void*, unsigned))(0x4602DCEC|1))
#define cmd_reboot ((void (*)(const char*, void*, unsigned))(0x4602B80C|1))
#define fastboot_register ((void (*)(const char*, void (*)(const char*, void*, unsigned), unsigned char))(0x4602AB90|1))