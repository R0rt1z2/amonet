#define GCPU_BASE 0x11018000

#define GCPU_REG_CTL              (GCPU_BASE + 0x000)
#define GCPU_REG_MSC              (GCPU_BASE + 0x004)

#define GCPU_AXI                  (GCPU_BASE + 0x020)
#define GCPU_UNK2                 (GCPU_BASE + 0x024)

#define GCPU_REG_PC_CTL           (GCPU_BASE + 0x400)
#define GCPU_REG_MEM_ADDR         (GCPU_BASE + 0x404)
#define GCPU_REG_MEM_DATA         (GCPU_BASE + 0x408)
#define GCPU_REG_READ_REG         (GCPU_BASE + 0x410)
#define GCPU_REG_MONCTL           (GCPU_BASE + 0x414)
#define GCPU_REG_DRAM_MON         (GCPU_BASE + 0x418)
#define GCPU_REG_CYC              (GCPU_BASE + 0x41C)
#define GCPU_REG_DRAM_INST_BASE   (GCPU_BASE + 0x420)

#define GCPU_REG_TRAP_START       (GCPU_BASE + 0x440)
#define GCPU_REG_TRAP_END         (GCPU_BASE + 0x478)

#define GCPU_REG_INT_SET          (GCPU_BASE + 0x800)
#define GCPU_REG_INT_CLR          (GCPU_BASE + 0x804)
#define GCPU_REG_INT_EN           (GCPU_BASE + 0x808)
#define GCPU_UNK3                 (GCPU_BASE + 0x80C)

#define GCPU_REG_MEM_CMD          (GCPU_BASE + 0xC00)
#define GCPU_REG_MEM_P0           (GCPU_BASE + 0xC04)
#define GCPU_REG_MEM_P1           (GCPU_BASE + 0xC08)
#define GCPU_REG_MEM_P2           (GCPU_BASE + 0xC0C)
#define GCPU_REG_MEM_P3           (GCPU_BASE + 0xC10)
#define GCPU_REG_MEM_P4           (GCPU_BASE + 0xC14)
#define GCPU_REG_MEM_P5           (GCPU_BASE + 0xC18)
#define GCPU_REG_MEM_P6           (GCPU_BASE + 0xC1C)
#define GCPU_REG_MEM_P7           (GCPU_BASE + 0xC20)
#define GCPU_REG_MEM_P8           (GCPU_BASE + 0xC24)
#define GCPU_REG_MEM_P9           (GCPU_BASE + 0xC28)
#define GCPU_REG_MEM_P10          (GCPU_BASE + 0xC2C)
#define GCPU_REG_MEM_P11          (GCPU_BASE + 0xC30)
#define GCPU_REG_MEM_P12          (GCPU_BASE + 0xC34)
#define GCPU_REG_MEM_P13          (GCPU_BASE + 0xC38)
#define GCPU_REG_MEM_P14          (GCPU_BASE + 0xC3C)
#define GCPU_REG_MEM_Slot         (GCPU_BASE + 0xC40)

#define GCPU_READ_REG(reg) (*(volatile uint32_t *)(reg))
#define GCPU_WRITE_REG(reg, val) ((*(volatile uint32_t *)(reg)) = (val))

#define TOP_CLOCK_CTRL_BASE 0x10000000

#define CLK_CFG_0                 (TOP_CLOCK_CTRL_BASE + 0x0140)
#define CLK_CFG_1                 (TOP_CLOCK_CTRL_BASE + 0x0144)
#define CLK_CFG_2                 (TOP_CLOCK_CTRL_BASE + 0x0148)
#define CLK_CFG_3                 (TOP_CLOCK_CTRL_BASE + 0x014C)
#define CLK_CFG_4                 (TOP_CLOCK_CTRL_BASE + 0x0150)
#define CLK_CFG_5                 (TOP_CLOCK_CTRL_BASE + 0x0154)
#define CLK_CFG_6                 (TOP_CLOCK_CTRL_BASE + 0x0158)
#define CLK_CFG_7                 (TOP_CLOCK_CTRL_BASE + 0x015C)
#define CLK_CFG_8                 (TOP_CLOCK_CTRL_BASE + 0x0164)
#define CLK_CFG_9                 (TOP_CLOCK_CTRL_BASE + 0x0168)

uint32_t hw_sha256(unsigned char *data_in_dram, unsigned int data_len,
                   unsigned int buffer_len,
                   unsigned char *sha256_result);


void gcpu_init(void);
void gcpu_acquire(void);
void gcpu_release(void);

int gcpu_cmd(uint32_t cmd);

void gcpu_memptr_set(uint32_t offset, uint8_t *data_in);
void gcpu_memptr_get(uint32_t offset, int len, uint8_t *data_out);

int gcpu_load_hw_key(uint32_t offset);
int gcpu_aes_decrypt(uint32_t key_offset, uint32_t data_offset, uint32_t out_offset);

int gcpu_aes_read16(uint32_t addr, uint8_t *out);

// Test cases
int gcpu_hw_sha256_test(void);