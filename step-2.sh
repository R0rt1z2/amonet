#!/bin/bash

PAYLOAD_BLOCK=223223

set -e

. functions.inc

if [ ! -f "gpt/gpt.bin.step2.gpt" ]; then
  echo "Couldn't find modified GPT, did you run step-1.sh first?"
  exit 1
fi

adb wait-for-device

get_root
set +e
echo "Looking for partition-suffix"
adb shell su -c \"ls -l /dev/block/platform/soc/by-name\" | grep recovery_tmp
if [ $? -ne 0 ] ; then
  adb shell su -c \"ls -l /dev/block/platform/soc/by-name\" | grep recovery_x
  if [ $? -ne 0 ] ; then
    echo "Didn't find new partitions, did you do step-1.sh first?"
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

echo "Flashing exploit"
adb push bin/boot.hdr /data/local/tmp/
adb push bin/boot.payload /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/boot.hdr of=/dev/block/platform/soc/by-name/boot${suffix} bs=512\" 
adb shell su -c \"dd if=/data/local/tmp/boot.payload of=/dev/block/platform/soc/by-name/boot${suffix} bs=512 seek=${PAYLOAD_BLOCK}\" 
adb shell su -c \"dd if=/data/local/tmp/boot.hdr of=/dev/block/platform/soc/by-name/recovery${suffix} bs=512\" 
adb shell su -c \"dd if=/data/local/tmp/boot.payload of=/dev/block/platform/soc/by-name/recovery${suffix} bs=512 seek=${PAYLOAD_BLOCK}\" 
echo ""

echo "Flashing Preloader"
adb push bin/boot0short.img /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/boot0short.img of=/dev/block/mmcblk0boot0 bs=512\" 
adb push bin/preloader.bin /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/preloader.bin of=/dev/block/mmcblk0boot0 bs=512 seek=520\" 
echo ""

echo "Flashing TZ"
adb shell su -c \"echo 0 \> /sys/block/mmcblk0boot0/force_ro\"
adb push bin/tz.img /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/tz.img of=/dev/block/platform/soc/by-name/tee1 bs=512\" 
adb shell su -c \"dd if=/data/local/tmp/tz.img of=/dev/block/platform/soc/by-name/tee2 bs=512\" 
echo ""

echo "Flashing LK"
adb push bin/lk.bin /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/lk.bin of=/dev/block/platform/soc/by-name/lk bs=512\" 
echo ""

echo "Flashing final GPT"
adb push gpt/gpt.bin.step2.gpt /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/gpt.bin.step2.gpt of=/dev/block/mmcblk0 bs=512 count=34\" 
if [ -f "gpt/gpt.bin.offset" ] ; then
  OFFSET=$(cat gpt/gpt.bin.offset)
  # Check if $OFFSET has some sane value
  if [ $OFFSET -gt 25000000 ] ; then
    echo "Flashing final GPT (backup)"
    adb push gpt/gpt.bin.step2.bak /data/local/tmp/
    adb shell su -c \"dd if=/data/local/tmp/gpt.bin.step2.bak of=/dev/block/mmcblk0 bs=512 seek=${OFFSET}\" 
  fi
fi
echo ""

echo "Flashing TWRP"
adb push bin/twrp.img /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/twrp.img of=/dev/block/platform/soc/by-name/recovery${suffix_b} bs=512\" 
echo ""

echo "Rebooting into TWRP"
adb reboot recovery
