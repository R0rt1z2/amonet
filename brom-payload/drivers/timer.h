#pragma once

#include <inttypes.h>

void timer_init(void);
void mdelay(unsigned long msec);
/* delay usec useconds */
void udelay(unsigned long usec);
uint32_t gpt4_get_current_tick(void);
uint32_t gpt4_tick2time_ms(uint32_t tick);
