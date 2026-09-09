#ifndef _MMU_H_
#define _MMU_H_

#include <inttypes.h>

#define MMU_SRAM_CACHED   0
#define MMU_SRAM_UNCACHED 1

uint32_t mmu_enable(int sram_uncached);

#endif
