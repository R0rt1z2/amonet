.syntax unified
.code 32

.section .bootloader, "a"
.global __bootloader_start
.global __bootloader_end
__bootloader_start:
.incbin "../bin/lk_crown.bin"
__bootloader_end:
