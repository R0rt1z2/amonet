.syntax unified
.thumb

.global start
.section .text.start
start:
    movw sp, #0x0000
    movt sp, #0x4900    @ sp = 0x42000000
    blx main