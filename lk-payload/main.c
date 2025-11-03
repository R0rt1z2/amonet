#include <inttypes.h>

#include "libc.h"

#include "common.h"
#include "bootimg.h"

extern uint8_t __bootloader_start[];
extern uint8_t __bootloader_end[];

void low_uart_put(int ch)
{
    volatile uint32_t *uart_reg0 = (volatile uint32_t *)0x11002014;
    volatile uint32_t *uart_reg1 = (volatile uint32_t *)0x11002000;

    while (!((*uart_reg0) & 0x20)) {}

    *uart_reg1 = ch;
}

void _putchar(char character)
{
    if (character == '\n')
        low_uart_put('\r');
    low_uart_put(character);
}

void arch_clean_invalidate_cache_range(uintptr_t start, uintptr_t size) {
    uintptr_t end = start + size;
    start &= ~(CACHE_LINE - 1);

    while (start < end) {
        __asm__ volatile(
                "mcr p15, 0, %0, c7, c14, 1\n"
                "add %0, %0, %[clsize]\n"
                : "+r"(start)
                : [clsize] "I"(CACHE_LINE)
                : "memory");
    }

    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4\n" ::"r"(0) : "memory");
}

void mmsys_reset(void) {
    // MMSYS_SW0_B_RST is the software reset register for display components      
    // including OVL, RDMA and DSI. We perform a software reset of all components 
    // (logic is inverted, so writing 0 asserts reset) to clear any configuration 
    // done by the first LK, ensuring the secondary LK has a clean state to work  
    // from as if it had started as the first LK.
    //
    // Thanks to @bengris32 and @TheVancedGamer for figuring this out!
    volatile uint32_t *reg = (volatile uint32_t *)MMSYS_SW0_B_RST;
    *reg = 0;
    __asm__ __volatile__("dmb sy" ::: "memory");
    *reg = 0xFFFFFFFF;
}

int main()
{
    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Updated version by k4y0z. Copyright 2019\n");
    printf("Ported to crown by R0rt1z2. Copyright 2025\n");

    uint32_t **argptr = (void *)0x4BD00020;
    uint32_t *arg = *argptr;

    printf("arg pointer: 0x%08x, arg value: 0x%08x\n", (uint32_t)arg, arg ? arg[0] : 0);

    size_t bootloader_size = __bootloader_end - __bootloader_start;
    void *lk_dst = (void*)0x4BD00000;

    printf("copying original LK from 0x%08x to 0x%08x, size 0x%08x bytes\n",
        (uint32_t)__bootloader_start + 0x200,
        (uint32_t)lk_dst,
        (uint32_t)(bootloader_size - 0x200));
    memcpy(lk_dst, __bootloader_start + 0x200, bootloader_size - 0x200);

    arch_clean_invalidate_cache_range((uintptr_t)lk_dst, LK_SIZE);
    
    __asm__ __volatile__("mcr p15, 0, %0, c7, c5, 0" :: "r"(0) : "memory");
    __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");

    printf("Resetting display subsystem\n");
    mmsys_reset();

    printf("Jumping to original LK\n");
    asm volatile (
        "cpsid if\n"
        "mov r4, %0\n"
        "mov r0, %0\n"
        "bx %1\n"
        : : "r" (arg), "r" (lk_dst) : "r0", "r4", "memory");

    while (1) {}
}