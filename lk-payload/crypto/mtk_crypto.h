#ifndef MTK_CRYPTO_H
#define MTK_CRYPTO_H

#include <stdint.h>

void mtk_crypto_hmac_sha256_by_devkey(uint8_t* data, uint32_t data_len, uint8_t* mac);

void mtk_crypto_hmac_sha256_by_devkey_using_seed(uint8_t* seed, uint8_t* data, uint32_t data_len, uint8_t* mac);

#endif