.syntax unified

.code 32

.global start
.section .text.start
start:
    mov sp, #0x82000000
    blx main
