.syntax unified

.code 32

.global start
.section .text.start
start:
    mov sp, #0x49000000
    blx main
