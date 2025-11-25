#pragma once

#include <stdint.h>

#define g_boot_mode ((uint32_t*)0x4609D120)

static inline uint32_t get_boot_mode(void) {
    return *g_boot_mode;
}

static inline void set_boot_mode(int mode) {
    *g_boot_mode = mode;
}