#pragma once

#include <stddef.h>
#include <stdint.h>

#include "thread.h"

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
int (*mtk_detect_key)(unsigned short key) = (void*)(0x4BD21C38 | 1);
int (*detect_power_key)() = (void*)(0x4bd22578 | 1);
void (*mdelay)(int) = (void*)(0x4BD21B58 | 1);

int (*thread_resume)(thread_t*) = (void*)(0x4BD31FCC | 1);
thread_t* (*thread_create)(const char*, void*, void*, int, size_t) = (void*)(0x4BD31E2C | 1);
void (*thread_sleep)(int) = (void*)(0x4BD32170 | 1);
void (*thread_exit)(int) = (void*)(0x4BD31EEC | 1);

void (*led_update)(int, uint8_t*) = (void*)(0x4BD329A0 | 1);
int (*led_write)(int, int) = (void*)(0x4BD3295C | 1);

void (*fastboot_info)(const char *reason) = (void *)(0x4bd34814 | 1);
void (*fastboot_fail)(const char *reason) = (void *)(0x4bd3485c | 1);
void (*fastboot_okay)(const char *reason) = (void *)(0x4bd34a20 | 1);

void (*fastboot_publish)(const char* name, const char* value) = (void *)(0x4bd34678 | 1);
void (*fastboot_register)(const char *prefix, 
                          void (*handle)(const char *arg, void *data, unsigned sz), 
                          unsigned char security_enabled) = (void *)(0x4bd345e4 | 1);

void (*cmd_flash)(const char *arg, void *data, unsigned sz) = (void *)(0x4bd36d68 | 1);

uint32_t* g_boot_mode = (uint32_t*) 0x4BD5C2AC; // LK boot mode
uint32_t* o_boot_mode = (uint32_t*) 0x4BE5E20C; // argptr boot mode

#define KEY_UBER 0

#define LK_SIZE (0x800 * 0x200)
#define LK_BASE 0x4BD00000

#define G_BOOT_ARG 0x4BD66360

#define CHAINLOAD_FLAG_BLOCK 1
#define UBOOT_BLOCK 2

#define UBOOT_ADDR 0x46000000
#define UBOOT_SIZE 0x200000

#define BOOT0_PART 1
#define BOOT1_PART 2
#define USER_PART 8
