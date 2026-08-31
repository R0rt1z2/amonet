#include "common.h"
#include "mmu.h"
#include "payload_common.h"

#include "printf.h"

#include "libc.h"

#include "drivers/types.h"
#include "drivers/core.h"
#include "drivers/mt_sd.h"
#include "drivers/errno.h"
#include "drivers/mmc.h"

#define PRELOADER_BASE 0x201000
#define PRELOADER_SIZE 0xE6000

#define SRAM_START 0x200000
#define SRAM_END   0x300000

#define BULK_CHUNK 0x3FC

uint16_t send_dword_pattern[]  = {0xB507, 0x0E03};
uint16_t recv_dword_pattern[]  = {0x4B0E, 0x2200};
uint16_t msdc_init_pattern[]  = {0xB570, 0x1E05, 0x460B};

int (*send_dword)();
void (*__recv_dword)();

int _recv_dword() {
    int dword = 0;
    __recv_dword(&dword);
    return dword;
}

int (*recv_dword)() = _recv_dword;

void (*usb_send)(char *addr, uint32_t sz) = 0;
int (*usb_recv)(char *addr, uint32_t sz, uint32_t tmo) = 0;

void _send_data(char *addr, uint32_t sz) {
    uint32_t total = (sz + 3) & ~3;

    if (usb_send) {
        for (uint32_t off = 0; off < total; ) {
            uint32_t chunk = total - off;
            if (chunk > BULK_CHUNK)
                chunk = BULK_CHUNK;
            if ((chunk & 0x1FF) == 0)
                chunk -= 4;
            usb_send(addr + off, chunk);
            off += chunk;
        }
        return;
    }

    for (uint32_t i = 0; i < total / 4; i++) {
        send_dword(__builtin_bswap32(((uint32_t *)addr)[i]));
    }
}

void _recv_data(char *addr, uint32_t sz, uint32_t flags __attribute__((unused))) {
    uint32_t total = (sz + 3) & ~3;

    if (usb_recv) {
        usb_recv(addr, total, 0);
        return;
    }

    for (uint32_t i = 0; i < total / 4; i++) {
        ((uint32_t *)addr)[i] = __builtin_bswap32(recv_dword());
    }
}

int (*send_data)() = (void *)_send_data;
int (*recv_data)() = (void *)_recv_data;

static uint32_t resolve_op(uint32_t ops, uint32_t index) {
    uint32_t fn = *(volatile uint32_t *)(ops + index * 4);

    if ((fn & 1) && fn > PRELOADER_BASE && fn < PRELOADER_BASE + PRELOADER_SIZE)
        return fn;

    return 0;
}

static void resolve_bulk_xfer(void) {
    uint32_t base = (uint32_t)send_dword & ~1u;
    uint32_t ctx_ptr = *(volatile uint32_t *)(base + 0x30) + base + 0x22;
    uint32_t ctx = *(volatile uint32_t *)ctx_ptr;

    if (ctx < SRAM_START || ctx >= SRAM_END)
        return;

    uint32_t ops = *(volatile uint32_t *)(ctx + 8);

    if (ops < SRAM_START || ops >= SRAM_END)
        return;

    usb_send = (void *)resolve_op(ops, 0);
    usb_recv = (void *)resolve_op(ops, 1);
}

uint32_t searchfunc(uint32_t startoffset, uint32_t endoffset, uint16_t *pattern, uint32_t patternsize) {
    uint32_t matched = 0;
    for (uint32_t offset = startoffset; offset < endoffset; offset += 2) {
        for (uint32_t i = 0; i < patternsize; i++) {
            if (((uint16_t *)offset)[i] != pattern[i]) {
                matched = 0;
                break;
            }
            if (++matched == patternsize) return offset;
        }
    }
    return 0;
}

int main() {
    printf("Entered preloader payload\n");
    printf("Copyright xyz, k4y0z 2021\n");

    struct msdc_host host = { 0 };
    host.ocr_avail = MSDC_OCR_AVAIL;

    mmc_init(&host);

    send_dword = (void *)(searchfunc(PRELOADER_BASE + 0x100, PRELOADER_BASE + 0x40000, send_dword_pattern, 2) | 1);
    printf("send_dword = %p\n", send_dword);
    __recv_dword = (void *)(searchfunc(PRELOADER_BASE + 0x100, PRELOADER_BASE + 0x40000, recv_dword_pattern, 2) | 1);
    printf("__recv_dword = %p\n", __recv_dword);

    resolve_bulk_xfer();
    printf("usb_send = %p usb_recv = %p\n", usb_send, usb_recv);

    void (*msdc_init)(uint32_t card, uint32_t unk) = (void *)(searchfunc(PRELOADER_BASE + 0x100, PRELOADER_BASE + 0x40000, msdc_init_pattern, 3) | 1);
    printf("msdc_init = %p\n", msdc_init);
    msdc_init(0, 2);

    printf("SCTLR = 0x%08X\n", mmu_enable());

    command_loop(&host);
}
