#include <inttypes.h>
#include <stdbool.h>

#include "libc.h"
#include "common.h"

#include "crypto/gcpu.h"
#include "crypto/mtk_crypto.h"
#include "crypto/hmac-sha256.h"

//#define RELOAD_LK

void low_uart_put(int ch) {
    volatile uint32_t *uart_reg0 = (volatile uint32_t*)0x11009014;
    volatile uint32_t *uart_reg1 = (volatile uint32_t*)0x11009000;

    while ( !((*uart_reg0) & 0x20) )
    {}

    *uart_reg1 = ch;
}

void _putchar(char character)
{
    if (character == '\n')
        low_uart_put('\r');
    low_uart_put(character);
}

uint32_t pmic_read_interface(uint32_t RegNum, uint32_t *val, uint32_t MASK, uint32_t SHIFT) {
    uint32_t ret = 0;
    uint32_t reg = 0;
    uint32_t rdata;

    ret = pwrap_wacs2(0, (RegNum), 0, &rdata);
    reg = rdata;
    if (ret != 0) {
        printf("Reg[0x%08X] = pmic_wrap read data fail\n", RegNum);
        return ret;
    }

    reg &= (MASK << SHIFT);
    *val = (reg >> SHIFT);
    return ret;
}

uint32_t is_usb_cable_in(void) {
    uint32_t nRet = 0;
    uint32_t nRegValue = 0;
    nRet = pmic_read_interface((uint32_t)(0x0000),
                             (&nRegValue),
                             (uint32_t)(0x1),
                             (uint32_t)(5));
    nRet = nRegValue;
    return nRet == 1;
}

bool is_power_key_pressed(void) {
    uint32_t nRet = 0;
    uint32_t nRegValue = 0;
    nRet = pmic_read_interface((uint32_t)(0x0144),
                             (&nRegValue),
                             (uint32_t)(0x1),
                             (uint32_t)(3));
    nRet = nRegValue;
    return nRet == 0;
}


int (*original_read)(part_dev_t *dev, uint64_t dev_addr, void *dst, uint32_t size) = (void*)(0x81e0a2d0|1);
int (*app)() = (void*)(0x81e3cb98|1);

uint64_t g_boot, g_boot_x, g_lk, g_misc, g_recovery, g_recovery_x;

int read_func(part_dev_t *dev, uint64_t block_off, void *dst, uint32_t sz) {
    printf("read_func hook\n");
    printf("block_off 0x%08X 0x%08X\n", block_off, *(&(block_off)+4));
    printf("dev 0x%08X dst 0x%08X sz 0x%08X\n", dev, dst, sz);
    int ret = 0;

    if (block_off == (g_boot * 0x200) + 0x1000000) {
        block_off = (g_boot_x * 0x200) + 0x1000000;
    } else if (block_off == ((g_boot * 0x200) + 0x800) + 0x1000000) {
        block_off = ((g_boot_x * 0x200) + 0x800) + 0x1000000;
    } else if (block_off == (g_recovery * 0x200) + 0x1000000) {
        block_off = (g_recovery_x * 0x200) + 0x1000000;
    } else if (block_off == ((g_recovery * 0x200) + 0x800) + 0x1000000) {
        block_off = ((g_recovery_x * 0x200) + 0x800) + 0x1000000;
    }

    return original_read(dev, block_off, dst, sz);
}

static void parse_gpt() {
    uint8_t raw[0x1000] = { 0 };
    part_dev_t *dev = get_device();
    dev->read(dev, 0x400 + 0x1000000, raw, sizeof(raw));

    for (int i = 0; i < sizeof(raw) / 0x80; ++i) {
        uint8_t *ptr = &raw[i * 0x80];
        uint8_t *name = ptr + 0x38;
        uint32_t start;
        memcpy(&start, ptr + 0x20, 4);
        if (memcmp(name, "b\x00o\x00o\x00t\x00\x00\x00", 10) == 0) {
            g_boot = start;
            printf("found boot at 0x%08X\n", start);
        } else if (memcmp(name, "b\x00o\x00o\x00t\x00_\x00x\x00\x00\x00", 14) == 0) {
            g_boot_x = start;
            printf("found boot_x at 0x%08X\n", start);
        } else if (memcmp(name, "U\x00\x42\x00O\x00O\x00T\x00\x00\x00", 12) == 0) {
            g_lk = start ;
            printf("found lk at 0x%08X\n", start);
        } else if (memcmp(name, "M\x00I\x00S\x00\x43\x00\x00\x00", 10) == 0) {
            g_misc = start;
            printf("found misc at 0x%08X\n", start);
        } else if (memcmp(name, "r\x00\x65\x00\x63\x00o\x00v\x00\x65\x00r\x00y\x00\x00\x00", 18) == 0) {
            g_recovery = start;
            printf("found recovery at 0x%08X\n", start);
        } else if (memcmp(name, "r\x00\x65\x00\x63\x00o\x00v\x00\x65\x00r\x00y\x00_\x00x\x00\x00\x00", 22) == 0) {
            g_recovery_x = start;
            printf("found recovery_x at 0x%08X\n", start);
        }
    }
}

static void byteswap(uint8_t *buf, size_t sz) {
    for (size_t i = 0; i < sz / 2; ++i) {
        size_t j = sz - i - 1;
        uint8_t o = buf[j];
        buf[j] = buf[i];
        buf[i] = o;
    }
}

static void derive_rpmb_key(uint8_t *in) {
    printf("in:\n");
    hex_dump(in, 0x10);
    printf("\n");

    uint8_t expand[64] = {0};
    for (int i = 0; i < 64; ++i)
    {
        expand[i] = (in)[i % 16];
    }

    printf("expand:\n");
    hex_dump(expand, 0x40);
    printf("\n");

    uint8_t result[0x20] = { 0 };
    mtk_crypto_hmac_sha256_by_devkey(expand, 0x40, result);

    printf("encrypted:\n");
    hex_dump(result, 0x20);

    uint8_t rpmb_key[0x20] = { 0 };
    hmac_sha256(rpmb_key, result, 0x20, (uint8_t *)"RPMB", 5);

    byteswap(rpmb_key, 0x20);

    printf("final:\n");
    hex_dump(rpmb_key, 0x20);
    printf("\n");
}

void read_memory(uint32_t addr, uint8_t *buffer, size_t length) {
    memcpy(buffer, (void *)addr, length);
}

void scan_memory() {
    uint8_t iv[16];
    uint8_t data[16];
    uint8_t decrypted_data[16];
    0x88000000
    0x1012ffc0

    for (uint32_t addr = 0x0; addr < 0x88000000; addr += 16) {
        // Fetch data using IV
        gcpu_aes_read16(addr, iv);

        // Fetch data by reading from the pointer
        read_memory(addr, data, 16);

        // Check if the fetched data is not zero
        if (memcmp(data, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) != 0) {
            printf("Address: 0x%08x\n", addr);

            // Print IV-decrypted data
            printf("IV-decrypted data:\n");
            hex_dump(iv, 16);

            // Print normal data
            printf("Normal data:\n");
            hex_dump(data, 16);
        }
    }
}

__attribute__((section(".text.start"))) int main() {
    int ret = 0;

    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Original version for sloane by k4y0z and t0x1cSH. Copyright 2020\n");
    printf("Ported to ariel ariel by R0rt1z2. Copyright 2024\n");

    // We need to clean the cache first, since we jumped straight to the payload
    cache_clean((void *)PAYLOAD_DST, PAYLOAD_SIZE);

    int fastboot = 0;

    parse_gpt();

    if (!g_boot_x || !g_recovery_x || !g_lk) {
        printf("failed to find boot, recovery or lk\n");
        printf("falling back to fastboot mode\n");
        fastboot = 1;
    }

    uint8_t bootloader_msg[0x20] = { 0 };

    part_dev_t *dev = get_device();

    // Restore the 0x81E00000-0x81E50000 range, a part of it was overwritten
    // this is way more than we actually need to restore, but it shouldn't hurt
    dev->read(dev, ((g_lk * 0x200) + 0x200) + 0x1000000, (char*)LK_BASE, 0x50000); // +0x200 to skip lk header

    // Restore argptr
    uint32_t **argptr = (void*)0x81e00020;
    *argptr = (void*)0x81E8549C; // there's also a copy at 0x81E80044?

    printf("g_boot_mode %u\n", *g_boot_mode);
    printf("f_boot_mode %u\n", *f_boot_mode);

    // factory and factory advanced boot
    if(*g_boot_mode == 4 ) {
      fastboot = 1;
    }

    // use advanced factory mode to boot recovery
    else if(*g_boot_mode == 6) {
      *g_boot_mode = 2;
    }

    else if(g_misc) {
      // Read amonet-flag from MISC partition
      dev->read(dev, (g_misc * 0x200) + 0x1000000, bootloader_msg, 0x50);
      printf("bootloader_msg: %s\n", bootloader_msg);

      // temp flag on MISC
      if(strncmp(bootloader_msg, "boot-amonet", 11) == 0) {
        fastboot = 1;
        // reset flag
        memset(bootloader_msg, 0, 0x10);
        dev->write(dev, bootloader_msg, (g_misc * 0x200) + 0x1000000, 0x10);
      }

      // recovery flag on MISC
      else if(strncmp(bootloader_msg, "boot-recovery", 13) == 0) {
        *g_boot_mode = 2;
        // reset flag
        memset(bootloader_msg, 0, 0x10);
        dev->write(dev, bootloader_msg, (g_misc * 0x200) + 0x1000000, 0x10);
      }

      // perm flag on MISC
      else if(strncmp(bootloader_msg, "FASTBOOT_PLEASE", 15) == 0) {
        // only reset flag in recovery-boot
        if(*g_boot_mode == 2) {
          memset(bootloader_msg, 0, 0x10);
          dev->write(dev, bootloader_msg, (g_misc * 0x200) + 0x1000000, 0x50);
        }
        else {
          fastboot = 1;
        }
      }

      // UART flag on MISC
      if(strncmp(bootloader_msg + 0x10, "UART_PLEASE", 11) == 0) {
        // Force uart enable
        char* disable_uart = (char*)0x81e612d8;
        strcpy(disable_uart, " printk.disable_uart=0");
        char* disable_uart2 = (char*)0x81e619a4;
        strcpy(disable_uart2, "printk.disable_uart=0");
      }

    }

#ifdef RELOAD_LK
      printf("Disable interrupts\n");
      asm volatile ("cpsid if");
#endif

    uint16_t *patch;

    if (is_usb_cable_in() && is_power_key_pressed()) {
        printf("USB cable is connected and power key is pressed\n");
        fastboot = 1;
    }

    // force fastboot mode
    if (fastboot) {
        printf("well since you're asking so nicely...\n");
        video_printf("=> HACKED FASTBOOT mode: (%d) - xyz, k4y0z, t0x1cSH, R0rt1z2\n", *g_boot_mode);
	    *g_boot_mode = 99;
    }
    else if(*g_boot_mode == 2) {
        video_printf("=> RECOVERY mode...");
        // cid = 0x15010046 0x4531324D 0x421167AA 0x4E6034E9
        // hardcode it for now
        /*uint32_t cid[4] = { 0x15010046, 0x4531324D, 0x421167AA, 0x4E6034E9 };
        printf("cid: 0x%08X 0x%08X 0x%08X 0x%08X\n", cid[0], cid[1], cid[2], cid[3]);
        uint32_t cid_be[4] = { 0 };
        for (int i = 0; i < 4; ++i) {
            cid_be[i] = __builtin_bswap32(cid[i]);
        }
        printf("cid_be: 0x%08X 0x%08X 0x%08X 0x%08X\n", cid_be[0], cid_be[1], cid_be[2], cid_be[3]);
        derive_rpmb_key((void*)cid_be);*/
        //printf("data:\n");
        //hex_dump(data, 0x10);
        gcpu_init();
        gcpu_acquire();
        //gcpu_load_hw_key(0x30);
        // use memptr set to set empty key
        // 16 zero array
        uint8_t empty_key[16] = {0x73, 0x5f, 0x23, 0xc9, 0x62, 0xe7, 0xa1, 0x0a,
                              0xb2, 0x01, 0xd9, 0xa6, 0x42, 0x60, 0x64, 0xb1};
        gcpu_memptr_set(0x12, empty_key);
        gcpu_aes_decrypt(0x12, 0x0, 0x1a);
        uint8_t dev_key[16] = {0};
        gcpu_memptr_get(0x12, 16, dev_key);
        printf("dev_key:\n");
        hex_dump(dev_key, 0x10);
        gcpu_release();

        gcpu_acquire();
        /*GCPU_WRITE_REG(GCPU_REG_MEM_P0, LK_BASE);
        GCPU_WRITE_REG(GCPU_REG_MEM_P1, 0x0); // dst to invalid pointer (otherwise, update pattern)
        GCPU_WRITE_REG(GCPU_REG_MEM_P2, 16);
        GCPU_WRITE_REG(GCPU_REG_MEM_P4, 18);
        GCPU_WRITE_REG(GCPU_REG_MEM_P5, 26);
        GCPU_WRITE_REG(GCPU_REG_MEM_P6, 26);*/

        uint8_t data[16] = {0};
        gcpu_aes_read16(0x12001000, data);

        /*int ret = gcpu_cmd(0x7E);
        if (ret != 0) {
            printf("aespk_d failed 0x%08X\n", ret);
            return ret;
        }

        uint8_t out[16] = { 0 };
        for (int i = 0; i < 4; i++) { // Read out the IV
            ((uint32_t *)out)[i] = GCPU_READ_REG(GCPU_REG_MEM_CMD + 26 * 4 + i * 4);
        }

        printf("IV:\n");
        hex_dump(out, 16);*/
        //volatile uint32_t *wdt = (volatile uint32_t *)0x10000000;
        /**
        dev.write32(0x10000000, 0x22000000)
        */
        printf("kick watchdog\n");
        GCPU_WRITE_REG(0x10000000, 0x22000000);
        scan_memory();

        gcpu_release();
    }

    // device is unlocked
    uint8_t **unlocked = (uint8_t**)0x81e81258;
    (*unlocked)[0x16] = 0x1;

    // printf("(void*)dev->read 0x%08X\n", (void*)dev->read);
    // printf("(void*)&dev->read 0x%08X\n", (void*)&dev->read);

    uint32_t *patch32;

    // hook bootimg read function
    original_read = (void*)dev->read;

    // patch32 = (void*)0x81E6C7C0;
    // *patch32 = (uint32_t)read_func;

    patch32 = (void*)&dev->read;
    *patch32 = (uint32_t)read_func;

    printf("Clean lk\n");
    cache_clean((void *)LK_BASE, LK_SIZE);

#ifdef RELOAD_LK
    printf("About to jump to LK\n");

    asm volatile (
        "mov r4, %0\n" 
        "mov r3, %1\n"
        "blx r3\n"
        : : "r" (arg), "r" (LK_BASE) : "r3", "r4");

    printf("Failure\n");
#else
    app();
#endif

    while (1) {

    }
}
