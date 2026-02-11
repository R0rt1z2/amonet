CC := arm-none-eabi-gcc
LD := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy

PAYLOAD_BLOCK := 223207

CFLAGS := -std=gnu99 -Os -mthumb -mcpu=cortex-a9 -fno-builtin-printf -fno-strict-aliasing -fno-builtin-memcpy -mno-unaligned-access -DPRINTF_DISABLE_SUPPORT_FLOAT=1
CFLAGS += -I. -I./bcbtool/include -DPAYLOAD_BLOCK=$(PAYLOAD_BLOCK)
LDFLAGS := -T linker.x -nodefaultlibs -nostdlib

BUILD_DIR := ./build

TARGET := payload

C_SRC = main.c debug.c libc.c bcbtool/lib/bcblib.c
ASM_SRC = start.S
ASM_SRC += $(wildcard libaeabi-cortexm0/*.S)

OBJ = $(C_SRC:%.c=$(BUILD_DIR)/%.o) $(ASM_SRC:%.S=$(BUILD_DIR)/%.o)
DEP = $(OBJ:%.o=%.d)

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $^ $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJ)
	$(LD) -o $@ $^ $(LDFLAGS)

-include $(DEP)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) -MMD -c -o $@ $< $(CFLAGS)

$(BUILD_DIR)/libaeabi-cortexm0/%.o: libaeabi-cortexm0/%.S
	mkdir -p $(@D)
	sed 's/\.cpu cortex-m0//' $< | $(CC) -c -o $@ -x assembler-with-cpp - $(CFLAGS)

$(BUILD_DIR)/%.o: %.S
	mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	-rm -rf $(BUILD_DIR)
