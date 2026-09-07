CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
LD := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy

CFLAGS := -std=gnu99 -Os -mthumb -mcpu=cortex-a9 -fno-builtin-printf -fno-strict-aliasing -fno-builtin-memcpy -mno-unaligned-access -Wall -Wextra
LDFLAGS := -T linker.x -nodefaultlibs -nostdlib -lgcc

BUILD_DIR := ./build

PL_ADDR := 0x201000
DA_ADDR := 0x40001000

COMMON_SRC = common.c payload_common.c libc.c printf.c drivers/sd.c drivers/mmc.c drivers/timer.c crypto/sha256.c crypto/hmac-sha256.c
ASM_SRC = start.S

PAYLOAD_SRC = main.c usb.c mmu.c $(COMMON_SRC)
PL_SRC = pl.c mmu.c $(COMMON_SRC)

PAYLOAD_OBJ = $(PAYLOAD_SRC:%.c=$(BUILD_DIR)/payload/%.o) $(ASM_SRC:%.S=$(BUILD_DIR)/payload/%.o)
PL_OBJ = $(PL_SRC:%.c=$(BUILD_DIR)/pl/%.o) $(ASM_SRC:%.S=$(BUILD_DIR)/pl/%.o)
DEP = $(PAYLOAD_OBJ:%.o=%.d) $(PL_OBJ:%.o=%.d)

all: $(BUILD_DIR)/payload.bin $(BUILD_DIR)/pl.bin

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary $^ $@

$(BUILD_DIR)/payload.elf: $(PAYLOAD_OBJ)
	$(LD) -o $@ $^ $(LDFLAGS) -Wl,--defsym=PAYLOAD_ADDR=$(PL_ADDR)

$(BUILD_DIR)/pl.elf: $(PL_OBJ)
	$(LD) -o $@ $^ $(LDFLAGS) -Wl,--defsym=PAYLOAD_ADDR=$(DA_ADDR)

-include $(DEP)

$(BUILD_DIR)/payload/%.o: %.c
	mkdir -p $(@D)
	$(CC) -MMD -c -o $@ $< $(CFLAGS)

$(BUILD_DIR)/payload/%.o: %.S
	mkdir -p $(@D)
	$(AS) -o $@ $<

$(BUILD_DIR)/pl/%.o: %.c
	mkdir -p $(@D)
	$(CC) -MMD -c -o $@ $< $(CFLAGS)

$(BUILD_DIR)/pl/%.o: %.S
	mkdir -p $(@D)
	$(AS) -o $@ $<

clean:
	-rm -rf $(BUILD_DIR)
