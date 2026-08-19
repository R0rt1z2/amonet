CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
LD := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy

IDMELIB_DIR := ../idmetool/lib

CFLAGS := -std=gnu99 -Os -mthumb -mcpu=cortex-a9 -fno-builtin-printf -fno-strict-aliasing -fno-builtin-memcpy -mno-unaligned-access -Wall -Wextra
CFLAGS += -I$(IDMELIB_DIR) -DIDMELIB_STANDALONE
LDFLAGS := -nodefaultlibs -nostdlib -lgcc

PL := pl

PL_SRC = pl.c common.c payload_common.c libc.c printf.c drivers/sd.c drivers/mmc.c crypto/sha256.c crypto/hmac-sha256.c
ASM_SRC = start.S

PL_OBJ = $(PL_SRC:%.c=$(PL)/%.o) $(ASM_SRC:%.S=$(PL)/%.o) $(PL)/idmelib.o
PL_DEP = $(PL_OBJ:%.o=%.d)

all: $(PL)/$(PL).bin

$(PL)/$(PL).bin: $(PL)/$(PL).elf
	$(OBJCOPY) -O binary $^ $@

$(PL)/$(PL).elf: $(PL_OBJ)
	$(LD) -o $@ $^ $(LDFLAGS) -T $(PL).ld

$(PL)/idmelib.o: $(IDMELIB_DIR)/idmelib.c
	mkdir -p $(@D)
	$(CC) -MMD -c -o $@ $< $(CFLAGS)

-include $(PL_DEP)

$(PL)/%.o: %.c
	mkdir -p $(@D)
	$(CC) -MMD -c -o $@ $< $(CFLAGS)

$(PL)/%.o: %.S
	mkdir -p $(@D)
	$(AS) -o $@ $<


clean:
	-rm -rf $(PL)
