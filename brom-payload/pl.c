#include "common.h"
#include "payload_common.h"

#define PRELOADER_BASE 0x201000
#define PRELOADER_SIZE 0x20000

static uint16_t send_dword_pattern[] = { 0xB507, 0x0E03 };
static uint16_t recv_dword_pattern[] = { 0x4B0E, 0x2200 };
static uint16_t msdc_init_pattern[] = { 0xB538, 0x4604, 0x4D0B };

static void (*__recv_dword)(uint32_t *dword);

static int _recv_dword(void) {
    uint32_t dword = 0;
    __recv_dword(&dword);
    return dword;
}

static void _send_data(char *addr, uint32_t sz) {
    for (uint32_t i = 0; i < (((sz + 3) & ~3) / 4); i++) {
        send_dword(__builtin_bswap32(((uint32_t *)addr)[i]));
    }
}

static void _recv_data(char *addr, uint32_t sz, uint32_t flags __attribute__((unused))) {
    for (uint32_t i = 0; i < (((sz + 3) & ~3) / 4); i++) {
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

int main() {
    printf("Entered the preloader payload\n");

    timer_init();

    send_dword = (void *)searchfunc(PRELOADER_BASE + 0x100, PRELOADER_BASE + PRELOADER_SIZE,
                                    send_dword_pattern, 2);
    __recv_dword = (void *)searchfunc(PRELOADER_BASE + 0x100, PRELOADER_BASE + PRELOADER_SIZE,
                                      recv_dword_pattern, 2);

    void (*msdc_init)(uint32_t card, uint32_t unk) =
        (void *)searchfunc(PRELOADER_BASE + 0x100, PRELOADER_BASE + PRELOADER_SIZE,
                           msdc_init_pattern, 3);

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

    struct msdc_host host = { 0 };
    host.ocr_avail = MSDC_OCR_AVAIL;

    mmc_init(&host);

    msdc_init(0, 2);

    command_loop(&host);
}
