#pragma once

#define BLK_BITS         (9)
#define BLK_SIZE         (1 << BLK_BITS)
#define BLK_NUM(size)    ((unsigned long long)(size) / BLK_SIZE)

typedef struct block_dev_desc {
    int             dev;
    unsigned long   lba;
    unsigned long   blksz;
    unsigned long   (*block_read)(int dev,
                                  unsigned long start,
                                  unsigned long blkcnt,
                                  void *buffer);
    unsigned long   (*block_write)(int dev,
                                   unsigned long start,
                                   unsigned long blkcnt,
                                   const void *buffer);
} block_dev_desc_t;

typedef struct part_dev part_dev_t;

struct part_dev {
    int init;
    int id;
    block_dev_desc_t *blkdev;
    int (*init_dev) (int id);
    int (*read)     (part_dev_t *dev, uint64_t src, unsigned char *dst, int size);
    int (*write)    (part_dev_t *dev, unsigned char *src, uint64_t dst, int size);
};

part_dev_t* (*get_device)() = (void*)(0x81e0a708|1);
void (*cache_clean)(void *addr, size_t sz) = (void*)0x81e1d1ac;
size_t (*video_printf)(const char *format, ...) = (void *)(0x81e3e910|1);
int (*pwrap_wacs2)(uint32_t write, uint32_t addr, uint32_t wdata, uint32_t *rdata) = (void*)(0x81e19600|1);

uint32_t* f_boot_mode = (uint32_t*) 0x81e825f8;
uint32_t* g_boot_mode = (uint32_t*) 0x81e6c414;

#define PAYLOAD_DST 0x81dfd000
#define PAYLOAD_SRC 0x80000
#define PAYLOAD_SIZE 0x80000

#define LK_BASE 0x81E00000
#define LK_SIZE (0x800 * 0x200)