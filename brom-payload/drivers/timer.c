#include <inttypes.h>
#include "timer.h"

#define APXGPT_BASE             0x10008000

#define GPT_IRQ_EN_REG          (APXGPT_BASE + 0x00)
#define GPT_IRQ_ACK_REG         (APXGPT_BASE + 0x08)
#define GPT_IRQ(n)              (1 << ((n) - 1))

#define GPT_CTRL_REG(n)         (APXGPT_BASE + (0x10 * (n)))
#define GPT_CLK_REG(n)          (APXGPT_BASE + 0x04 + (0x10 * (n)))
#define GPT_CNT_REG(n)          (APXGPT_BASE + 0x08 + (0x10 * (n)))
#define GPT_CMP_REG(n)          (APXGPT_BASE + 0x0C + (0x10 * (n)))

#define GPT_CTRL_OP(val)        (((val) & 0x3) << 4)
#define GPT_CTRL_OP_FREERUN     3
#define GPT_CTRL_CLEAR          2
#define GPT_CTRL_ENABLE         1
#define GPT_CTRL_DISABLE        0

#define GPT_CLK_SRC(val)        (((val) & 0x1) << 4)
#define GPT_CLK_SRC_SYS13M      0
#define GPT_CLK_DIV1            0

#define GPT4                    4

#define GPT4_1US_TICK           13
#define GPT4_1MS_TICK           13000

static inline uint32_t readl(uint32_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void writel(uint32_t val, uint32_t addr)
{
    *(volatile uint32_t *)addr = val;
}

void timer_init(void)
{
    writel(GPT_CTRL_CLEAR | GPT_CTRL_DISABLE, GPT_CTRL_REG(GPT4));
    writel(GPT_CLK_SRC(GPT_CLK_SRC_SYS13M) | GPT_CLK_DIV1, GPT_CLK_REG(GPT4));
    writel(0, GPT_CMP_REG(GPT4));
    writel(readl(GPT_IRQ_EN_REG) & ~GPT_IRQ(GPT4), GPT_IRQ_EN_REG);
    writel(GPT_IRQ(GPT4), GPT_IRQ_ACK_REG);
    writel(GPT_CTRL_OP(GPT_CTRL_OP_FREERUN) | GPT_CTRL_ENABLE, GPT_CTRL_REG(GPT4));
}

uint32_t gpt4_get_current_tick(void)
{
    return readl(GPT_CNT_REG(GPT4));
}

uint32_t gpt4_tick2time_ms(uint32_t tick)
{
    return tick / GPT4_1MS_TICK;
}

void mdelay(unsigned long ms)
{
    uint32_t start = readl(GPT_CNT_REG(GPT4));
    uint32_t timeout = ms * GPT4_1MS_TICK;

    while ((readl(GPT_CNT_REG(GPT4)) - start) < timeout)
        ;
}

void udelay(unsigned long us)
{
    uint32_t start = readl(GPT_CNT_REG(GPT4));
    uint32_t timeout = us * GPT4_1US_TICK;

    while ((readl(GPT_CNT_REG(GPT4)) - start) < timeout)
        ;
}
