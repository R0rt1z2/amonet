#pragma once

struct device_t {
    uint32_t unk1;
    uint32_t unk2;
    uint32_t unk3;
    uint32_t unk4;
    size_t (*read)(struct device_t *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(struct device_t *dev, void *src, uint64_t block_off, size_t size, uint32_t part);
};

struct device_t* (*get_device)() = (void*)0x4BD14699;
void (*cache_clean)(void *addr, size_t sz) = (void*)0x4BD2A6DC;
size_t (*video_printf)(const char *format, ...) = (void *)0x4BD30A1F;

uint32_t* g_boot_mode = (uint32_t*) 0x4BD69354;

#define PAYLOAD_DST 0x41000000
#define PAYLOAD_SRC 0x80000
#define PAYLOAD_SIZE 0x80000

#define BOOT0_PART 1
#define USER_PART 8
