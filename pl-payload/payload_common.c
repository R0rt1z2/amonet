#include "common.h"
#include "payload_common.h"

#include "idmelib.h"

static uint8_t idme_buf[IDME_SIZE];

#define XFER_BLOCKS_MAX     64

static uint8_t xfer_buf[XFER_BLOCKS_MAX * 0x200];

void sleepy(void) {
    // TODO: do better
    for (volatile int i = 0; i < 0x80000; ++i) {}
}

void mdelay (unsigned long msec)
{
    (void)msec;
    sleepy();
}

/* delay usec useconds */
void udelay (unsigned long usec)
{
    (void)usec;
    sleepy();
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

    printf("Switch to boot1 => ");
    printf("0x%08X\n", mmc_set_part(host, 2));
    mdelay(500);

    for (uint32_t block = 0; block < IDME_NUM_BLOCKS; block++) {
        if (mmc_read(host, block, idme_buf + block * IDME_MMC_BLOCK_SIZE) != 0) {
            printf("Read error!\n");
            goto out;
        }
    }

    idme = (struct idme *)idme_buf;

out:
    printf("Switch back to user => ");
    printf("0x%08X\n", mmc_set_part(host, 0));
    mdelay(500);

    return idme;
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
        uint32_t cmd = recv_dword();
        switch (cmd) {
        case 0x1000: {
            uint32_t block = recv_dword();
            printf("Read block 0x%08X\n", block);
            memset(buf, 0, sizeof(buf));
            if (mmc_read(host, block, buf) != 0) {
                printf("Read error!\n");
            } else {
                send_data(buf, sizeof(buf));
            }
            break;
        }
        case 0x1001: {
            uint32_t block = recv_dword();
            printf("Write block 0x%08X ", block);
            memset(buf, 0, sizeof(buf));
            recv_data(buf, 0x200, 0);
            if (mmc_write(host, block, buf) != 0) {
                printf("Write error!\n");
            } else {
                printf("OK\n");
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

            printf("Read 0x%08X blocks at 0x%08X ", blocks, block);

            if (mmc_read_blocks(host, block, xfer_buf, blocks) != 0) {
                printf("Read error!\n");
            } else {
                printf("OK\n");
                send_data(xfer_buf, blocks * 0x200);
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

            printf("Write 0x%08X blocks at 0x%08X ", blocks, block);
            recv_data(xfer_buf, blocks * 0x200, 0);

            if (mmc_write_blocks(host, block, xfer_buf, blocks) != 0) {
                printf("Write error!\n");
            } else {
                printf("OK\n");
                send_dword(0xD0D0D0D0);
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
            volatile uint32_t *reg = (volatile uint32_t *)0x10007000;
            reg[8/4] = 0x1971;
            reg[0/4] = 0x22000014;
            reg[0x14/4] = 0x1209;

            while (1) {

            }
        }
        case 0x3001: {
            printf("Kick watchdog\n");
            volatile uint32_t *reg = (volatile uint32_t *)0x10007000;
            reg[8/4] = 0x1971;
            break;
        }
        default:
            printf("Invalid command\n");
            break;
        }
    }

    printf("Exiting the payload\n");

    while (1) {

    }
}
