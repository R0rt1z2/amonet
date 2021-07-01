#!/bin/bash

PAYLOAD_BLOCK=223223

set -e

. functions.inc

adb wait-for-device

check_device "giza" " - Amazon Fire HD 8 (2016) - "

get_root

serialno=`adb shell getprop ro.boot.serialno | dos2unix`

set +e
echo "Looking for partition-suffix"
adb shell su -c \"ls -l /dev/block/platform/soc/by-name\" | grep recovery_tmp
if [ $? -ne 0 ] ; then
  adb shell su -c \"ls -l /dev/block/platform/soc/by-name\" | grep recovery_x
  if [ $? -ne 0 ] ; then
    echo "Didn't find new partitions, did you do step-1.sh first?"
    exit 1
  else
    echo "Found \"_x\" suffix, it looks like you are rerunning step-2.sh"
    echo "If this is expected, press enter, otherwise terminate with Ctrl+C"
    read
    suffix=""
    suffix_b="_x"
  fi
else
  suffix="_tmp"
  suffix_b=""
fi
set -e
echo ""

if [ ! -f "gpt-${serialno}/gpt.bin.step2.gpt" ]; then
  echo "Couldn't find GPT files, regenerating from device"
  echo ""

  echo "Dumping GPT"
  [ ! -d gpt-${serialno}-regen ] && mkdir gpt-${serialno}-regen
  adb shell su -c \"dd if=/dev/block/mmcblk0 bs=512 count=34 of=/data/local/tmp/gpt.bin\" 
  adb shell su -c \"chmod 644 /data/local/tmp/gpt.bin\" 
  adb pull /data/local/tmp/gpt.bin gpt-${serialno}-regen/gpt.bin
  echo ""

  echo "Unpatching GPT"
  modules/gpt.py unpatch gpt-${serialno}-regen/gpt.bin
  [ ! -d gpt-${serialno} ] && mkdir gpt-${serialno}
  cp gpt-${serialno}-regen/gpt.bin.unpatched.gpt gpt-${serialno}/gpt.bin
  echo ""

  echo "Modifying GPT"
  modules/gpt.py patch gpt-${serialno}/gpt.bin
  echo ""
fi


echo "Flashing exploit"
adb push bin/boot.hdr /data/local/tmp/
adb push bin/boot.payload /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/boot.hdr of=/dev/block/platform/soc/by-name/boot${suffix} bs=512\" 
adb shell su -c \"dd if=/data/local/tmp/boot.payload of=/dev/block/platform/soc/by-name/boot${suffix} bs=512 seek=${PAYLOAD_BLOCK}\" 
adb shell su -c \"dd if=/data/local/tmp/boot.hdr of=/dev/block/platform/soc/by-name/recovery${suffix} bs=512\" 
adb shell su -c \"dd if=/data/local/tmp/boot.payload of=/dev/block/platform/soc/by-name/recovery${suffix} bs=512 seek=${PAYLOAD_BLOCK}\" 
echo ""

echo "Flashing LK"
adb push bin/lk.bin /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/lk.bin of=/dev/block/platform/soc/by-name/lk bs=512\" 
echo ""

echo "Flashing TZ"
adb push bin/tz.img /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/tz.img of=/dev/block/platform/soc/by-name/tee1 bs=512\" 
adb shell su -c \"dd if=/data/local/tmp/tz.img of=/dev/block/platform/soc/by-name/tee2 bs=512\" 
echo ""

echo "Flashing Preloader"
adb shell su -c \"echo 0 \> /sys/block/mmcblk0boot0/force_ro\"
adb push bin/boot0short.img /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/boot0short.img of=/dev/block/mmcblk0boot0 bs=512\" 
adb push bin/preloader.bin /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/preloader.bin of=/dev/block/mmcblk0boot0 bs=512 seek=520\" 
echo ""

echo "Flashing final GPT"
adb push gpt-${serialno}/gpt.bin.step2.gpt /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/gpt.bin.step2.gpt of=/dev/block/mmcblk0 bs=512 count=34\" 
echo ""
if [ -f "gpt-${serialno}/gpt.bin.offset" ] ; then
  OFFSET=$(cat gpt-${serialno}/gpt.bin.offset)
  # Check if $OFFSET has some sane value
  if [ $OFFSET -gt 25000000 ] ; then
    echo "Flashing final GPT (backup)"
    adb push gpt-${serialno}/gpt.bin.step2.bak /data/local/tmp/
    adb shell su -c \"dd if=/data/local/tmp/gpt.bin.step2.bak of=/dev/block/mmcblk0 bs=512 seek=${OFFSET}\" 
    echo ""
  fi
fi

echo "Flashing TWRP"
adb push bin/twrp.img /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/twrp.img of=/dev/block/platform/soc/by-name/recovery${suffix_b} bs=512\" 
echo ""

echo "Rebooting into TWRP"
adb reboot recovery
