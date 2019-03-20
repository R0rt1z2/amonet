#!/bin/bash

set -e

. functions.inc

adb wait-for-device

get_root

echo "Your device will be reset to factory defaults..."
echo "Press Enter to Continue..."
read

echo "Dumping GPT"
[ ! -d gpt ] && mkdir gpt
adb shell su -c \"dd if=/dev/block/mmcblk0 bs=512 count=34 of=/data/local/tmp/gpt.bin\" 
adb shell su -c \"chmod 644 /data/local/tmp/gpt.bin\" 
adb pull /data/local/tmp/gpt.bin gpt/gpt.bin

echo "Modifying GPT"
modules/gpt.py gpt/gpt.bin

echo "Flashing temp GPT"
adb push gpt/gpt.bin.step1.gpt /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/gpt.bin.step1.gpt of=/dev/block/mmcblk0 bs=512 count=34\" 

echo "Preparing for Factory Reset"
adb shell su -c \"mkdir -p /cache/recovery\"
adb shell su -c \"echo --wipe_data \> /cache/recovery/command\"
adb shell su -c \"echo --wipe_cache \>\> /cache/recovery/command\"

echo "Rebooting into recovery"
adb reboot recovery
