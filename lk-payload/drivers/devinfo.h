#ifndef DEVINFO_H
#define DEVINFO_H

#include <stdint.h>

#include "read.h"

#define sdr_read32(reg)          __raw_readl((const volatile void *)reg)

/* Read dev_info registers */
uint32_t get_devinfo_with_index(uint32_t index);

#endif