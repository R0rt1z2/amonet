#pragma once

struct device_t {
    uint32_t unk1;
    uint32_t unk2;
    uint32_t unk3;
    uint32_t unk4;
    size_t (*read)(struct device_t *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(struct device_t *dev, void *src, uint64_t block_off, size_t size, uint32_t part);
};

struct device_t* (*get_device)() = (void*)0x4BD2B2F1;
void (*cache_clean)(void *addr, size_t sz) = (void*)0x4BD31444;

uint32_t* g_boot_mode = (uint32_t*) 0x4BD5C2AC;

#define PAYLOAD_DST 0x41000000
#define PAYLOAD_SRC 0x200000
#define PAYLOAD_SIZE 0x200000

#define BOOT0_PART 1
#define USER_PART 8
