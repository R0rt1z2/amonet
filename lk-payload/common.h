#pragma once

#ifdef __has_include
  #if __has_include(<stdint.h>)
    #define HAS_STDINT_H 1
  #endif
#endif

#if defined(_STDINT_H) || defined(__STDINT_H) || defined(_STDINT_H_) || defined(HAS_STDINT_H)
  #include <stdint.h>
  #ifndef NULL
    #define NULL ((void *)0)
  #endif
  #ifndef BLOCK_SIZE
    #define BLOCK_SIZE 512
  #endif
  typedef unsigned char bool;
  #define true ((bool)1)
  #define false ((bool)0)
#else
  #define NULL ((void *)0)
  #define BLOCK_SIZE 512
  #define true ((bool)1)
  #define false ((bool)0)

  typedef char bool;
  typedef char int8_t;
  typedef short int16_t;
  typedef int int32_t;
  typedef long long int64_t;

  typedef unsigned char uint8_t;
  typedef unsigned short uint16_t;
  typedef unsigned int uint32_t;
  typedef unsigned long long uint64_t;

  typedef uint32_t size_t;
#endif

typedef struct __part_dev part_dev_t;

struct __part_dev
{
    uint32_t init;
    uint32_t id;
    void *blkdev;
    void *init_dev;
    int (*read)(part_dev_t *dev, uint64_t src, void *dst, int size, unsigned int part_id);
    void *write;
};

int (*app)() = (void*)(0x4602a1c0|1);
void (*video_printf)(const char *fmt, ...) = (void (*)(const char *fmt, ...))(0x46032DAC | 1);
void (*dprintf)(const char *fmt, ...) = (void (*)(const char* fmt, ...))(0x460333B8 | 1);
void (*arch_clean_invalidate_cache_range)(uint32_t start, uint32_t size) = (void (*)(uint32_t, uint32_t))0x4601c5cc;
part_dev_t *(*mt_part_get_device)() = (part_dev_t * (*)())(0x46053740 | 1);
int (*bldr_load_dtb)(char *boot_load_partition) = (int (*)(char *))(0x460276e8 | 1);
int (*is_volume_down_pressed)(void) = (int (*)(void))(0x46000160 | 1);
int (*is_volume_up_pressed)(void) = (int (*)(void))(0x46000150 | 1);

part_dev_t *get_device(void) {
    return mt_part_get_device();
}

uint32_t* g_boot_mode = (uint32_t*) 0x4609d120;

#define PAYLOAD_OFFSET 0x200000
#define PAYLOAD_ADDR 0x42000000
#define PAYLOAD_SIZE (16384*3)

#define BOOT0_PART 1
#define USER_PART 8

#define LK_BASE 0x46000000
#define LK_SIZE (0x800 * 0x200)