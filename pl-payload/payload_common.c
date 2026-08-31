#include "common.h"
#include "payload_common.h"

#include "idmelib.h"

#define WDT_BASE        0x10007000
#define WDT_MODE        (WDT_BASE + 0x00)
#define WDT_RESTART     (WDT_BASE + 0x08)
#define WDT_SWRST       (WDT_BASE + 0x14)

#define WDT_RESTART_KEY 0x1971
#define WDT_MODE_KEY    0x22000014
#define WDT_SWRST_KEY   0x1209

#define XFER_BLOCKS_MAX 1024

static uint8_t idme_buf[IDME_SIZE];
static uint8_t xfer_buf[XFER_BLOCKS_MAX * 0x200];

void sleepy(void) {
    for (volatile int i = 0; i < 0x80000; ++i) {}
}

void mdelay (unsigned long msec)
{
    (void)msec;
    sleepy();
}

void udelay (unsigned long usec)
{
    (void)usec;
    sleepy();
}

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

static struct idme *read_idme(struct msdc_host *host) {
    struct idme *idme = 0;

    if (mmc_set_part(host, 2) != 0) {
        printf("Switch to boot1 failed!\n");
        return 0;
    }

    if (mmc_read_blocks(host, 0, idme_buf, IDME_NUM_BLOCKS) != 0)
        printf("Read error!\n");
    else
        idme = (struct idme *)idme_buf;

    if (mmc_set_part(host, 0) != 0)
        printf("Switch back to user failed!\n");

    return idme;
}

void command_loop(struct msdc_host *host) {
    char buf[0x200] = { 0 };
    int ret = 0;

    printf("Entering command loop\n");

    send_dword(0xB1B2B3B4);

    while (1) {
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
            if (mmc_read(host, block, buf) != 0)
                printf("Read error at block 0x%08X!\n", block);
            else
                send_data(buf, sizeof(buf));
            break;
        }
        case 0x1001: {
            uint32_t block = recv_dword();
            memset(buf, 0, sizeof(buf));
            recv_data(buf, 0x200, 0);
            if (mmc_write(host, block, buf) != 0)
                printf("Write error at block 0x%08X!\n", block);
            else
                send_dword(0xD0D0D0D0);
            break;
        }
        case 0x1004: {
            uint32_t block = recv_dword();
            uint32_t blocks = recv_dword();

            if (blocks == 0 || blocks > XFER_BLOCKS_MAX) {
                printf("Refusing to read 0x%08X blocks\n", blocks);
                break;
            }

            if (mmc_read_blocks(host, block, xfer_buf, blocks) != 0)
                printf("Read error at block 0x%08X!\n", block);
            else
                send_data(xfer_buf, blocks * 0x200);
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

            if (mmc_write_blocks(host, block, xfer_buf, blocks) != 0)
                printf("Write error at block 0x%08X!\n", block);
            else
                send_dword(0xD0D0D0D0);
            break;
        }
        case 0x1002: {
            uint32_t part = recv_dword();
            printf("Switch to partition %d => ", part);
            ret = mmc_set_part(host, part);
            printf("0x%08X\n", ret);
            break;
        }
        case 0x1005: {
            send_dword(XFER_BLOCKS_MAX);
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
            printf("Read %d Bytes from address 0x%08X\n", size, address);
            send_data((char*)address, size);
            break;
        }
        case 0x7000: {
            char name[IDME_MAX_NAME_LEN] = { 0 };
            struct idme_item *item;
            struct idme *idme;
            uint32_t size;

            recv_data(name, sizeof(name), 0);
            printf("Read %s from IDME\n", name);

            idme = read_idme(host);
            if (!idme) {
                send_dword(4);
                send_dword(0xFFFFFFFF);
                break;
            }

            if (!idmelib_magic_valid(idme)) {
                printf("IDME invalid!\n");
                send_dword(4);
                send_dword(0xBEEFDEED);
                break;
            }

            item = idmelib_get_item(idme, name);
            if (!item) {
                printf("%s not found in IDME\n", name);
                send_dword(4);
                send_dword(0xDEADBEEF);
                break;
            }

            size = item->desc.size;
            if (size > sizeof(buf)) {
                printf("%s is 0x%08X bytes, truncating\n", name, size);
                size = sizeof(buf);
            }

            memset(buf, 0, sizeof(buf));
            memcpy(buf, item->data, size);
            send_dword(size);
            send_data(buf, (size + 3) & ~3);
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
