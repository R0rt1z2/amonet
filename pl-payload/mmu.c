#include "mmu.h"

#define SECTION_SIZE    0x100000
#define SECTION_COUNT   4096

#define DESC_SECTION    (2 << 0)
#define DESC_AP_RW      (3 << 10)
#define DESC_SHAREABLE  (1 << 16)
#define DESC_XN         (1 << 4)
#define DESC_TEX_WB     (1 << 12)
#define DESC_C          (1 << 3)
#define DESC_B          (1 << 2)

#define DESC_NORMAL     (DESC_SECTION | DESC_AP_RW | DESC_SHAREABLE | DESC_TEX_WB | DESC_C | DESC_B)
#define DESC_DEVICE     (DESC_SECTION | DESC_AP_RW | DESC_SHAREABLE | DESC_XN | DESC_B)

#define TTBR0_ATTRS     0x4A
#define DACR_CLIENT     0x55555555

#define SCTLR_M         (1 << 0)
#define SCTLR_C         (1 << 2)
#define SCTLR_I         (1 << 12)

#define SRAM_LIMIT      0x00400000
#define DRAM_BASE       0x40000000

static uint32_t page_table[SECTION_COUNT] __attribute__((aligned(16384)));

static void dcache_invalidate_all(void) {
    uint32_t clidr, loc, level;

    __asm__ volatile ("mrc p15, 1, %0, c0, c0, 1" : "=r" (clidr));
    loc = (clidr >> 24) & 7;

    for (level = 0; level < loc; level++) {
        uint32_t ccsidr, lsize, assoc, nsets, wayshift, way, set;

        if (((clidr >> (level * 3)) & 7) < 2)
            continue;

        __asm__ volatile ("mcr p15, 2, %0, c0, c0, 0" :: "r" (level << 1));
        __asm__ volatile ("isb");
        __asm__ volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));

        lsize = (ccsidr & 7) + 4;
        assoc = (ccsidr >> 3) & 0x3FF;
        nsets = (ccsidr >> 13) & 0x7FFF;
        wayshift = assoc ? (uint32_t)__builtin_clz(assoc) : 31;

        for (way = 0; way <= assoc; way++) {
            for (set = 0; set <= nsets; set++) {
                uint32_t val = (way << wayshift) | (set << lsize) | (level << 1);
                __asm__ volatile ("mcr p15, 0, %0, c7, c6, 2" :: "r" (val));
            }
        }
    }

    __asm__ volatile ("mcr p15, 2, %0, c0, c0, 0" :: "r" (0));
    __asm__ volatile ("dsb" ::: "memory");
}

uint32_t mmu_enable(void) {
    uint32_t sctlr;

    for (uint32_t i = 0; i < SECTION_COUNT; i++) {
        uint32_t base = i * SECTION_SIZE;

        if (base < SRAM_LIMIT || base >= DRAM_BASE)
            page_table[i] = base | DESC_NORMAL;
        else
            page_table[i] = base | DESC_DEVICE;
    }

    dcache_invalidate_all();

    __asm__ volatile (
        "mcr p15, 0, %0, c8, c7, 0\n"
        "mcr p15, 0, %0, c7, c5, 0\n"
        "mcr p15, 0, %0, c7, c5, 6\n"
        "dsb\n"
        "isb\n"
        :: "r" (0) : "memory");

    __asm__ volatile ("mcr p15, 0, %0, c2, c0, 2" :: "r" (0));
    __asm__ volatile ("mcr p15, 0, %0, c2, c0, 0" :: "r" ((uint32_t)page_table | TTBR0_ATTRS));
    __asm__ volatile ("mcr p15, 0, %0, c3, c0, 0" :: "r" (DACR_CLIENT));
    __asm__ volatile ("isb" ::: "memory");

    __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (sctlr));
    sctlr |= SCTLR_M | SCTLR_C | SCTLR_I;
    __asm__ volatile ("mcr p15, 0, %0, c1, c0, 0" :: "r" (sctlr) : "memory");
    __asm__ volatile ("isb" ::: "memory");

    __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (sctlr));
    return sctlr;
}
