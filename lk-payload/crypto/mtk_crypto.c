#include "mtk_crypto.h"
#include "gcpu.h"
#include "hmac-sha256.h"

#include "../drivers/devinfo.h"
#include "../libc.h"

const uint8_t init_array[] = {0x73, 0x5f, 0x23, 0xc9, 0x62, 0xe7, 0xa1, 0x0a,
                              0xb2, 0x01, 0xd9, 0xa6, 0x42, 0x60, 0x64, 0xb1};

void xor_data(uint8_t *a, uint8_t *b, uint8_t *out_buf, int len) {
    int i;
    for (i = 0; i < len; i++) {
        out_buf[i] = a[i] ^ b[i];
    }
}

void mtk_crypto_hmac_sha256_by_devkey_using_seed(uint8_t *seed, uint8_t *data,
                                                 uint32_t data_len,
                                                 uint8_t *mac) {
    uint8_t dev_key[16] = {0};

    gcpu_acquire();

    if (!gcpu_load_hw_key(0x30)) {
        gcpu_memptr_set(0x12, seed);
        if (!gcpu_aes_decrypt(0x30, 0x12, 0x1a)) {
            gcpu_memptr_get(0x1a, 16, dev_key);
            printf("scrambled key:\n");
            hex_dump(dev_key, 0x10);
            printf("\n");

        } else {
            printf("gcpu_aes_decrypt failed\n");
        }
    } else {
        printf("gcpu_load_hw_key failed\n");
    }

    gcpu_release();

    hmac_sha256(mac, data, data_len, dev_key, 16);
}

void mtk_crypto_hmac_sha256_by_devkey(uint8_t *data, uint32_t data_len,
                                      uint8_t *mac) {
    uint8_t seed[16] = {0};
    uint32_t dev_val;

    memcpy(seed, init_array, 16);

    dev_val = get_devinfo_with_index(12);
    xor_data(seed, (uint8_t *)&dev_val, seed, 4);
    dev_val = get_devinfo_with_index(13);
    xor_data(seed + 4, (uint8_t *)&dev_val, seed + 4, 4);

    printf("seed:\n");
    hex_dump(seed, 16);
    printf("\n");

    mtk_crypto_hmac_sha256_by_devkey_using_seed(seed, data, data_len, mac);
}