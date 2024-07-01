#!/bin/bash

. functions.inc

adb wait-for-device

product=$(adb shell getprop ro.product.name | dos2unix)

if [[ "$product" != *"ariel"* ]]; then
    echo "This script is only for devices containing the name 'ariel' (e.g., Amazon Fire HD6 / HD7 (2014)), your device is a \"${product}\"."
    exit 1
fi

echo "This will restore your GPT and install TWRP recovery."
echo "This will also downgrade to 4.5.3 bootloader."
echo "Press Enter to Continue..."
read

set +e
echo "Looking for partition-suffix"
adb shell su -c \"ls -l /dev/block/platform/mtk-msdc.0/by-name\" | grep recovery_tmp
if [ $? -ne 0 ] ; then
  adb shell su -c \"ls -l /dev/block/platform/mtk-msdc.0/by-name\" | grep recovery_x
  if [ $? -ne 0 ] ; then
    echo "Didn't find modified gpt, nothing to do."
    exit 1
  else
    suffix=""
    suffix_b="_x"
  fi
else
  suffix="_tmp"
  suffix_b=""
fi
set -e
echo ""

echo "Dumping GPT"
[ ! -d gpt-regen ] && mkdir gpt-regen
adb shell su -c \"dd if=/dev/block/mmcblk0 bs=512 count=34 of=/data/local/tmp/gpt.bin\"
adb shell su -c \"chmod 644 /data/local/tmp/gpt.bin\"
adb pull /data/local/tmp/gpt.bin gpt-regen/gpt.bin
echo ""

echo "Unpatching GPT"
modules/gpt.py unpatch gpt-regen/gpt.bin
[ ! -d gpt ] && mkdir gpt
cp gpt-regen/gpt.bin.unpatched.gpt gpt/gpt.bin
cp gpt-regen/gpt.bin.unpatched.bak gpt/gpt.bin.bak
cp gpt-regen/gpt.bin.offset gpt/gpt.bin.offset
echo ""

echo "Flashing TWRP recovery"
adb push bin/twrp.img /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/twrp.img of=/dev/block/platform/mtk-msdc.0/by-name/recovery${suffix_b} bs=512\"
echo ""

echo "Flashing old bootloader"
adb push bin/453_lk.bin /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/453_lk.bin of=/dev/block/platform/mtk-msdc.0/by-name/UBOOT bs=512\" # Better safe than sorry
adb shell su -c \"dd if=/data/local/tmp/453_lk.bin of=/dev/block/platform/mtk-msdc.0/by-name/UBOOT_real bs=512\" 2>/dev/null
echo ""

echo "Flashing unpatched GPT"
adb push gpt/gpt.bin /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/gpt.bin of=/dev/block/mmcblk0 bs=512 count=34\"
echo ""
if [ -f "gpt/gpt.bin.offset" ] ; then
  OFFSET=$(cat gpt/gpt.bin.offset)
  # Check if $OFFSET has some sane value
  if [ $OFFSET -gt 25000000 ] ; then
    echo "Flashing unpatched GPT (backup)"
    adb push gpt/gpt.bin.bak /data/local/tmp/
    adb shell su -c \"dd if=/data/local/tmp/gpt.bin.bak of=/dev/block/mmcblk0 bs=512 seek=${OFFSET}\"
    echo ""
  fi
fi

echo "Rebooting TWRP Recovery"
adb reboot recovery
