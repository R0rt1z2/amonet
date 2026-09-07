#include "common.h"
#include "payload_common.h"

#define WDT_BASE            0x10007000
#define WDT_MODE            (WDT_BASE + 0x00)
#define WDT_RESTART         (WDT_BASE + 0x08)
#define WDT_SWRST           (WDT_BASE + 0x14)

#define WDT_RESTART_KEY     0x1971
#define WDT_MODE_KEY        0x22000014
#define WDT_SWRST_KEY       0x1209

#define XFER_BLOCKS_MAX     64

static uint8_t xfer_buf[XFER_BLOCKS_MAX * 0x200] __attribute__((aligned(4)));

static void wdt_kick(void) {
    *(volatile uint32_t *)WDT_RESTART = WDT_RESTART_KEY;
}

void hex_dump(const void* data, size_t size) {
    size_t i, j;
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*)data)[i]);
        if ((i+1) % 8 == 0 || i+1 == size) {
            printf(" ");
            if ((i+1) % 16 == 0) {
                printf("\n");
            } else if (i+1 == size) {
                if ((i+1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("\n");
            }
        }
    }
}

void command_loop(struct msdc_host *host) {
    char buf[0x200] = { 0 };
    int ret = 0;

    printf("Entering command loop\n");

    send_dword(0xB1B2B3B4);

    while (1) {
        memset(buf, 0, sizeof(buf));
        uint32_t magic = recv_dword();
        if (magic != 0xf00dd00d) {
            printf("Protocol error\n");
            printf("Magic received = 0x%08X\n", magic);
            break;
        }
        wdt_kick();

        uint32_t cmd = recv_dword();
        switch (cmd) {
        case 0x1000: {
            uint32_t block = recv_dword();
            memset(buf, 0, sizeof(buf));
            if (mmc_read(host, block, buf) != 0) {
                printf("Read error at block 0x%08X!\n", block);
            } else {
                send_data(buf, sizeof(buf));
            }
            break;
        }
        case 0x1001: {
            uint32_t block = recv_dword();
            memset(buf, 0, sizeof(buf));
            recv_data(buf, 0x200, 0);
            if (mmc_write(host, block, buf) != 0) {
                printf("Write error at block 0x%08X!\n", block);
            } else {
                send_dword(0xD0D0D0D0);
            }
            break;
        }
        case 0x1003: {
            uint32_t block = recv_dword();
            uint32_t blocks = recv_dword();

            if (blocks == 0 || blocks > XFER_BLOCKS_MAX) {
                printf("Refusing to write 0x%08X blocks\n", blocks);
                break;
            }

            recv_data(xfer_buf, blocks * 0x200, 0);

            if (mmc_write_blocks(host, block, xfer_buf, blocks) != 0) {
                printf("Write error at block 0x%08X!\n", block);
            } else {
                send_dword(0xD0D0D0D0);
            }
            break;
        }
        case 0x1004: {
            uint32_t block = recv_dword();
            uint32_t blocks = recv_dword();

            if (blocks == 0 || blocks > XFER_BLOCKS_MAX) {
                printf("Refusing to read 0x%08X blocks\n", blocks);
                break;
            }

            if (mmc_read_blocks(host, block, xfer_buf, blocks) != 0) {
                printf("Read error at block 0x%08X!\n", block);
            } else {
                send_data(xfer_buf, blocks * 0x200);
            }
            break;
        }
        case 0x1002: {
            uint32_t part = recv_dword();
            printf("Switch to partition %d => ", part);
            ret = mmc_set_part(host, part);
            printf("0x%08X\n", ret);
            mdelay(500); // just in case
            break;
        }
        case 0x2000: {
            printf("Read rpmb\n");
            mmc_rpmb_read(host, buf);
            send_data(buf, 0x100);
            break;
        }
        case 0x2001: {
            printf("Write rpmb\n");
            recv_data(buf, 0x100, 0);
            mmc_rpmb_write(host, buf);
            break;
        }
        case 0x5000: {
            uint32_t address = recv_dword();
            uint32_t size = recv_dword();
            send_data((char *)address, (size + 3) & ~3);
            break;
        }
        case 0x5002: {
            fast_send_set(1);

            for (uint32_t i = 0; i < 0x200; i++)
                xfer_buf[i] = (uint8_t)(i * 7);

            send_data(xfer_buf, 0x200);
            break;
        }
        case 0x5003: {
            fast_send_set(0);
            send_dword(0xD0D0D0D0);
            break;
        }
        case 0x3000: {
            printf("Reboot\n");
            *(volatile uint32_t *)WDT_RESTART = WDT_RESTART_KEY;
            *(volatile uint32_t *)WDT_MODE = WDT_MODE_KEY;
            *(volatile uint32_t *)WDT_SWRST = WDT_SWRST_KEY;

            while (1) {

            }
        }
        case 0x3001:
            break;
        default:
            printf("Invalid command\n");
            break;
        }
    }

    printf("Exiting the payload\n");

    while (1) {

    }
}
