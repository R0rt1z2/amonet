#pragma once

struct device_t {
    uint32_t unk1;
    uint32_t unk2;
    uint32_t unk3;
    uint32_t unk4;
    size_t (*read)(struct device_t *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(struct device_t *dev, void *src, uint64_t block_off, size_t size, uint32_t part);
};

struct device_t* (*get_device)() = (void*)0x81e0a700;
void (*cache_clean)(void *addr, size_t sz) = (void*)0x81e1d1a4;
size_t (*video_printf)(const char *format, ...) = (void *)0x81e3e5ac;
size_t (*dprintf)(const char *format, ...) = (void *)0x81e3e7f4;


uint32_t* f_boot_mode = (uint32_t*) 0x81e81450;
uint32_t* g_boot_mode = (uint32_t*) 0x81e6c414;

#define PAYLOAD_DST 0x81dff000
#define PAYLOAD_SRC 0x80000
#define PAYLOAD_SIZE 0x80000

#define LK_BASE 0x81E00000
#define LK_SIZE (0x800 * 0x200)

#define BOOT0_PART 1
#define USER_PART 8
