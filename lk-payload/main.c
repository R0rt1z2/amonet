#include <inttypes.h>

#include "libc.h"

#include "common.h"

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

uint32_t pmic_config_interface(uint32_t reg, uint32_t val, uint32_t mask, uint32_t shift)
{
    return ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t))(0x4bd137f8 |1))(reg, val, mask, shift);
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

uint64_t g_swdl;

static void parse_gpt(struct device_t *dev) {
    uint8_t raw[0x800] = { 0 };
    dev->read(dev, 0x400, raw, sizeof(raw), USER_PART);
    
    for (int i = 0; i < sizeof(raw) / 0x80; ++i) {
        uint8_t *ptr = &raw[i * 0x80];
        uint8_t *name = ptr + 0x38;
        uint64_t start;
        memcpy(&start, ptr + 0x20, 8);
        
        if (start == 0) continue;

        if (strwcmp(name, "swdl") == 0) {
            printf("found swdl at 0x%08X\n", start);
            g_swdl = start;
        }
    }
}

void mmsys_reset(void) {
    /*
    // MMSYS_SW0_B_RST is the software reset register for display components      
    // including OVL, RDMA and DSI. We perform a software reset of all components 
    // (logic is inverted, so writing 0 asserts reset) to clear any configuration 
    // done by the first LK, ensuring the secondary LK has a clean state to work  
    // from as if it had started as the first LK.
    //
    // Thanks to @bengris32 and @TheVancedGamer for figuring this out!
    */
    volatile uint32_t *reg = (volatile uint32_t *)MMSYS_SW0_B_RST;
    *reg = 0;
    __asm__ __volatile__("dmb sy" ::: "memory");
    *reg = 0xFFFFFFFF;
}

int main()
{
    int ret;

    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Updated version by k4y0z. Copyright 2019\n");
    printf("Ported to crown by R0rt1z2. Copyright 2025\n");
    printf("Built on %s at %s\n", __DATE__, __TIME__);

    uint32_t **argptr = (void *)0x4BD00020;
    uint32_t *arg = *argptr;

    printf("arg pointer: 0x%08x, arg value: 0x%08x\n", (uint32_t)arg, arg ? arg[0] : 0);

    void *lk_tmp = (void*)LK_TEMP;
    void *lk_dst = (void*)LK_BASE;

    struct device_t *dev = get_device();
    parse_gpt(dev);

    if (!g_swdl) {
        printf("failed to find swdl\n");
        while (1) {}
    }

    printf("Backup microloader\n");
    memcpy((void *)MICROLOADER_BACKUP, (void *)MICROLOADER_SRC, MICROLOADER_SIZE);

    ret = dev->read(dev, g_swdl * 0x200 + 0x200, lk_tmp, LK_SIZE, USER_PART);
    if (ret < 0) {
        printf("Failed to read original LK from storage\n");
        while (1) {}
    }

    printf("Reset MMSYS\n");
    mmsys_reset();

    printf("Disable long press power off\n");
    pmic_config_interface(0x011A, 0x0, 0x1, 6);

    printf("Disable interrupts\n");
    asm volatile ("cpsid if");

    printf("Copy original LK\n");
    memcpy(lk_dst, lk_tmp, LK_SIZE);

    printf("Clean LK cache\n");
    arch_clean_invalidate_cache_range((uintptr_t)lk_dst, LK_SIZE);
    __asm__ __volatile__("mcr p15, 0, %0, c7, c5, 0" :: "r"(0) : "memory");
    __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");

    printf("About to jump to LK\n");
    asm volatile (
        "cpsid if\n"
        "mov r4, %0\n"
        "mov r0, %0\n"
        "bx %1\n"
        : : "r" (arg), "r" (lk_dst) : "r0", "r4", "memory");

    printf("Failure\n");
    while (1) {}
}