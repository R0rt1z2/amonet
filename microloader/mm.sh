#!/bin/sh
python3 inject_microloader.py lk_patched_header.bin ../lk-payload/build/payload.bin test.fin
dd if=test.fin of=microloader.hdr bs=1 count=$((0x60))
cp microloader.hdr microloader.fb.hdr
echo -n "FASTBOOT_PLEASE\0" >> microloader.fb.hdr
dd if=test.fin of=microloader.payload bs=1 skip=$((0x1000000)) count=$((0x2000))
#dd if=test.fin of=microloader.payload bs=1 skip=$((0x1000010)) count=$((0x2000))
dd if=test.fin of=microloader.tail bs=1 skip=$((0x6D00000))
#dd if=test.fin of=microloader.tail bs=1 skip=$((0x6CFFE00))
cp microloader.hdr ../bin/
cp microloader.fb.hdr ../bin/
cp microloader.payload ../bin/
cp microloader.tail ../bin/
