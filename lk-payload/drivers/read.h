#pragma once

#include <inttypes.h>

static inline uint16_t __raw_readw(const volatile void *addr)
{
    uint16_t val;
    asm volatile("ldrh %0, %1"
             : "=r" (val)
             : "Q" (*(volatile uint16_t *)addr));
    return val;
}

static inline uint8_t __raw_readb(const volatile void *addr)
{
    uint8_t val;
    asm volatile("ldrb %0, %1"
             : "=r" (val)
             : "Qo" (*(volatile uint8_t *)addr));
    return val;
}

static inline uint32_t __raw_readl(const volatile void *addr)
{
    uint32_t val;
    asm volatile("ldr %0, %1"
             : "=r" (val)
             : "Qo" (*(volatile uint32_t *)addr));
    return val;
}