#include "common.h"
#include "payload_common.h"

int (*send_dword)() = (void*)0xC047;
int (*recv_dword)() = (void*)0xC013;
// addr, sz
int (*send_data)() = (void*)0xC10F;
// addr, sz, flags (=0)
int (*recv_data)() = (void*)0xC089;

int main() {
    // Restore the pointer we overwrote
    uint32_t *ptr_send = (void*)0x1028A8;
    *ptr_send = 0x5FE5;

    printf("Entered the payload\n");

    timer_init();

    struct msdc_host host = { 0 };
    host.ocr_avail = MSDC_OCR_AVAIL;

    mmc_init(&host);

    mmc_enable_8bit(&host);

    command_loop(&host);
}
