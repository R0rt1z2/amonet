#include "common.h"
#include "mmu.h"
#include "payload_common.h"

#define PRELOADER_BASE 0x201000
#define PRELOADER_SIZE 0x23000

#define SRAM_START     0x100000
#define SRAM_END       0x300000

#define BULK_CHUNK     0x3FC

static uint16_t send_dword_pattern[] = { 0xB507, 0x0E03 };
static uint16_t recv_dword_pattern[] = { 0x4B0E, 0x2200 };

static uint16_t msdc_init_patterns[][3] = {
    { 0xB538, 0x4604, 0x4D0B },
    { 0xB538, 0x4605, 0x4C0B },
};

static void (*__recv_dword)(uint32_t *dword);

static void (*usb_send)(char *addr, uint32_t sz);
static int (*usb_recv)(char *addr, uint32_t sz, uint32_t tmo);

static int _recv_dword(void) {
    uint32_t dword = 0;
    __recv_dword(&dword);
    return dword;
}

static void _send_data(char *addr, uint32_t sz) {
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

static void _recv_data(char *addr, uint32_t sz, uint32_t flags __attribute__((unused))) {
    uint32_t total = (sz + 3) & ~3;

    if (usb_recv) {
        usb_recv(addr, total, 0);
        return;
    }

    for (uint32_t i = 0; i < total / 4; i++) {
        ((uint32_t *)addr)[i] = __builtin_bswap32(recv_dword());
    }
}

int (*send_dword)();
int (*recv_dword)() = (void*)_recv_dword;
// addr, sz
int (*send_data)() = (void*)_send_data;
// addr, sz, flags (=0)
int (*recv_data)() = (void*)_recv_data;

static uint32_t searchfunc(uint32_t start, uint32_t end, uint16_t *pattern, uint32_t size) {
    uint32_t matched = 0;
    for (uint32_t offset = start; offset < end; offset += 2) {
        for (uint32_t i = 0; i < size; i++) {
            if (((uint16_t *)offset)[i] != pattern[i]) {
                matched = 0;
                break;
            }
            if (++matched == size) return offset;
        }
    }
    return 0;
}

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

void fast_send_set(int on __attribute__((unused))) {
}

int main() {
    printf("Entered the preloader payload\n");

    timer_init();

    send_dword = (void *)searchfunc(PRELOADER_BASE + 0x100, PRELOADER_BASE + PRELOADER_SIZE,
                                    send_dword_pattern, 2);
    __recv_dword = (void *)searchfunc(PRELOADER_BASE + 0x100, PRELOADER_BASE + PRELOADER_SIZE,
                                      recv_dword_pattern, 2);

    void (*msdc_init)(uint32_t card, uint32_t unk) = 0;

    for (uint32_t i = 0; i < sizeof(msdc_init_patterns) / sizeof(msdc_init_patterns[0]); i++) {
        msdc_init = (void *)searchfunc(PRELOADER_BASE + 0x100, PRELOADER_BASE + PRELOADER_SIZE,
                                       msdc_init_patterns[i], 3);
        if (msdc_init)
            break;
    }

    printf("send_dword = %p, __recv_dword = %p, msdc_init = %p\n",
           send_dword, __recv_dword, msdc_init);

    if (!send_dword || !__recv_dword || !msdc_init) {
        printf("Failed to find the preloader functions, giving up\n");
        while (1) {

        }
    }

    send_dword = (void *)((uint32_t)send_dword | 1);
    __recv_dword = (void *)((uint32_t)__recv_dword | 1);
    msdc_init = (void *)((uint32_t)msdc_init | 1);

    resolve_bulk_xfer();

    printf("usb_send = %p, usb_recv = %p\n", usb_send, usb_recv);

    struct msdc_host host = { 0 };
    host.ocr_avail = MSDC_OCR_AVAIL;

    mmc_init(&host);

    msdc_init(0, 2);

    printf("SCTLR = 0x%08X\n", mmu_enable(MMU_SRAM_UNCACHED));

    command_loop(&host);
}
