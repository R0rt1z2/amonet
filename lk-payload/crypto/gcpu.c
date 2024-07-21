#include <inttypes.h>
#include <stdbool.h>

#include "../libc.h"
#include "gcpu.h"
#include "sha256.h"

void gcpu_init(void) {
    printf("gcpu_init\n");
    uint32_t keyslot = 0x12;
    uint32_t ivslot = 0x1A;

    GCPU_WRITE_REG(GCPU_REG_MEM_P2, 0x0);
    GCPU_WRITE_REG(GCPU_REG_MEM_P3, 0x0);
    GCPU_WRITE_REG(GCPU_REG_MEM_P4, 0x0);
    GCPU_WRITE_REG(GCPU_REG_MEM_P5, 0x0);
    GCPU_WRITE_REG(GCPU_REG_MEM_P6, 0x0);
    GCPU_WRITE_REG(GCPU_REG_MEM_P7, 0x0);
    GCPU_WRITE_REG(GCPU_REG_MEM_P8, 0x0);
    GCPU_WRITE_REG(GCPU_REG_MEM_P9, 0x0);
    GCPU_WRITE_REG(GCPU_REG_MEM_P10, 0x0);

    uint32_t zeros_4[4] = {0, 0, 0, 0};
    uint32_t zeros_8[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    for (int i = 0; i < 4; i++) {
        GCPU_WRITE_REG(GCPU_REG_MEM_CMD + (keyslot * 4) + (i * 4), 0x0);
        GCPU_WRITE_REG(GCPU_REG_MEM_CMD + (22 * 4) + (i * 4), 0x0);
    }
    for (int i = 0; i < 8; i++) {
        GCPU_WRITE_REG(GCPU_REG_MEM_CMD + (ivslot * 4) + (i * 4), 0x0);
    }
}

void gcpu_acquire(void) {
    printf("gcpu_acquire\n");
    GCPU_WRITE_REG(CLK_CFG_8,
                   (GCPU_READ_REG(CLK_CFG_8) & 0xf8ffffff) | 0x1000000);
    GCPU_WRITE_REG(GCPU_BASE, (GCPU_READ_REG(GCPU_BASE) & 0xfffffff0) | 0xf);
    GCPU_WRITE_REG(GCPU_AXI, 0x885b);
}

void gcpu_release(void) {
    printf("gcpu_release\n");
    GCPU_WRITE_REG(CLK_CFG_8, GCPU_READ_REG(CLK_CFG_8) & 0xf8ffffff);
    GCPU_WRITE_REG(GCPU_BASE, (GCPU_READ_REG(GCPU_BASE) & 0xfffffff0) | 0xf);
    GCPU_WRITE_REG(GCPU_AXI, 0x885b);
}

int gcpu_cmd(uint32_t cmd) {
    GCPU_WRITE_REG(GCPU_REG_INT_CLR, 3);
    GCPU_WRITE_REG(GCPU_REG_INT_EN, 3);
    GCPU_WRITE_REG(GCPU_REG_MEM_CMD, cmd);
    GCPU_WRITE_REG(GCPU_REG_PC_CTL, 0);

    while (GCPU_READ_REG(GCPU_REG_INT_SET) == 0) {
    }

    if (GCPU_READ_REG(GCPU_REG_INT_SET) & 2) {
        if (GCPU_READ_REG(GCPU_REG_INT_SET) & 1) {
            while (GCPU_READ_REG(GCPU_REG_INT_SET) == 0) {
            }
            GCPU_WRITE_REG(GCPU_REG_INT_CLR, 3);
            return -1;
        }
    } else {
        while ((GCPU_READ_REG(GCPU_REG_DRAM_MON) & 1) == 0) {
        }
        GCPU_WRITE_REG(GCPU_REG_INT_CLR, 3);
        return 0;
    }
    return -1;
}

void gcpu_memptr_set(uint32_t offset, uint8_t *data_in) {
    int i;

    for (i = 0; i < 16; i += 4) {
        GCPU_WRITE_REG(GCPU_REG_MEM_CMD + i + (offset * 4),
                       ((uint32_t *)(data_in + i))[0]);
    }
}

void gcpu_memptr_get(uint32_t offset, int len, uint8_t *data_out) {
    int i;

    for (i = 0; i < len; i += 4) {
        ((uint32_t *)(data_out + i))[0] =
            GCPU_READ_REG(GCPU_REG_MEM_CMD + i + (offset * 4));
    }
}

int gcpu_load_hw_key(uint32_t offset) {
    printf("gcpu_load_hw_key\n");
    GCPU_WRITE_REG(GCPU_REG_MEM_P0, 0x58);   // src start address
    GCPU_WRITE_REG(GCPU_REG_MEM_P1, offset); // dst start address
    GCPU_WRITE_REG(GCPU_REG_MEM_P2, 4);      // size

    return gcpu_cmd(0x70);
}

int gcpu_aes_decrypt(uint32_t key_offset, uint32_t data_offset,
                     uint32_t out_offset) {
    GCPU_WRITE_REG(GCPU_REG_MEM_P0, 1); // src
    GCPU_WRITE_REG(GCPU_REG_MEM_P1, key_offset); // dst
    GCPU_WRITE_REG(GCPU_REG_MEM_P2, data_offset);
    GCPU_WRITE_REG(GCPU_REG_MEM_P3, out_offset);

    return gcpu_cmd(0x78);
}

int gcpu_aes_read16(uint32_t addr, uint8_t *out) {
    //printf("gcpu_aes_read16 addr: 0x%08X\n", addr);

    GCPU_WRITE_REG(GCPU_REG_MEM_P0, addr);
    GCPU_WRITE_REG(GCPU_REG_MEM_P1, 0x0); // dst to invalid pointer (otherwise, update pattern)
    GCPU_WRITE_REG(GCPU_REG_MEM_P2, 1);
    GCPU_WRITE_REG(GCPU_REG_MEM_P4, 18);
    GCPU_WRITE_REG(GCPU_REG_MEM_P5, 26);
    GCPU_WRITE_REG(GCPU_REG_MEM_P6, 26);

    int ret = gcpu_cmd(0x7E);
    if (ret != 0) {
        printf("aespk_d failed 0x%08X\n", ret);
        return ret;
    }

    for (int i = 0; i < 4; i++) { // Read out the IV
        ((uint32_t *)out)[i] = GCPU_READ_REG(GCPU_REG_MEM_CMD + 26 * 4 + i * 4);
    }

    return 0;
}

int gcpu_hw_sha256_test() {
    unsigned char buffer[1088] __attribute__((aligned(64)));
    unsigned char swsha256[32];
    unsigned char hwsha256[32];

    for (int i = 0; i < 1024; i++) {
        buffer[i] = (unsigned char)i;
    }
    memset(buffer + 1024, 0, 1088 - 1024);

    printf("scrambled data (skipping %u bytes):\n", (1024 - 100));
    hex_dump(buffer, 100);

    uint32_t ret = ((uint32_t(*)(unsigned char *, unsigned int, unsigned int,
                                 unsigned char *))(0x81e4d064 | 1))(
        buffer, 1024, 1088, hwsha256);
    if (ret == 0) {
        printf("hw_sha256 hash:\n");
        hex_dump(hwsha256, 32);
    } else {
        printf("hw_sha256 failed\n");
        return ret;
    }

    sha256_hash(swsha256, buffer, 1024);
    printf("sw_sha256 hash:\n");
    hex_dump(swsha256, 32);

    bool success = true;
    for (int i = 0; i < 32; i++) {
        if (swsha256[i] != hwsha256[i]) {
            success = false;
            break;
        }
    }

    printf("SHA256 test %s!!\n", success ? "passed" : "failed");
    return success ? 0 : 1;
}