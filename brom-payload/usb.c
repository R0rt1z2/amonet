#include "common.h"
#include "libc.h"
#include "usb.h"

#include "drivers/timer.h"

#define USB_BASE             0x11200000
#define USB_POWER            (USB_BASE + 0x01)
#define USB_INDEX            (USB_BASE + 0x0E)
#define USB_TXCSR            (USB_BASE + 0x12)
#define USB_FIFO(ep)         (USB_BASE + 0x20 + 4 * (ep))

#define USB_POWER_HS         (1 << 4)
#define USB_TXCSR_FLUSH      (1 << 3)

#define USB_STATE            ((volatile uint8_t *)(0x0010365C + 0x1E5))
#define USB_STATE_CONFIGURED 3
#define USB_EP_PTR           (*(volatile uint32_t *)0x0010316C)
#define USB_TX_COUNT         (*(volatile uint32_t *)0x00105A68)
#define USB_DONE             ((volatile uint8_t *)0x001027D8)

#define USB_SEND_TIMEOUT     (100 * 13000)

#define SRAM_START           0x100000
#define SRAM_END             0x110000

static void (*usb_start)(uint32_t ep) = (void*)0x6DC7;
static void (*usb_poll)(void) = (void*)0x6E47;

static int (*brom_send_data)() = (void*)0xC10F;

static int fast_send = 0;

static uint32_t usb_endpoint(void) {
    uint32_t ep_ptr = USB_EP_PTR;

    if (ep_ptr < SRAM_START || ep_ptr >= SRAM_END)
        return 0;

    return *(volatile uint8_t *)ep_ptr;
}

static int usb_send_packet(uint32_t ep, const char *addr, uint32_t len) {
    volatile uint32_t *fifo32 = (volatile uint32_t *)USB_FIFO(ep);
    volatile uint8_t *fifo8 = (volatile uint8_t *)USB_FIFO(ep);
    uint32_t i = 0;
    uint32_t start;

    *(volatile uint8_t *)USB_INDEX = ep;

    if (((uint32_t)addr & 3) == 0)
        for (; i + 4 <= len; i += 4)
            *fifo32 = *(const uint32_t *)(addr + i);

    for (; i < len; i++)
        *fifo8 = (uint8_t)addr[i];

    *USB_DONE = 0;
    usb_start(ep);

    start = gpt4_get_current_tick();

    while (!*USB_DONE) {
        if (gpt4_get_current_tick() - start > USB_SEND_TIMEOUT) {
            *(volatile uint8_t *)USB_INDEX = ep;
            *(volatile uint16_t *)USB_TXCSR |= USB_TXCSR_FLUSH;
            return -1;
        }
        usb_poll();
    }

    return 0;
}

void usb_send_data(char *addr, uint32_t sz) {
    uint32_t total = (sz + 3) & ~3;
    uint32_t off = 0;
    uint32_t ep, pkt;

    if (!fast_send)
        goto tail;

    ep = usb_endpoint();
    pkt = (*(volatile uint8_t *)USB_POWER & USB_POWER_HS) ? 512 : 64;

    if (ep == 0 || ep > 15 || *USB_STATE != USB_STATE_CONFIGURED || USB_TX_COUNT != 0)
        goto tail;

    while (total - off >= pkt) {
        if (usb_send_packet(ep, addr + off, pkt) != 0) {
            fast_send = 0;
            printf("Fast send stalled, falling back\n");
            break;
        }
        off += pkt;
    }

tail:
    if (off < total)
        brom_send_data(addr + off, total - off);
}

void fast_send_set(int on) {
    fast_send = on;
}
