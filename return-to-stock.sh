#!/bin/bash

. functions.inc

adb wait-for-device

product=$(adb shell getprop ro.product.name | dos2unix)

if [ "$product" != "giza" ] ; then
  echo "This is only for the \"giza\" (Amazon Fire HD8 (2016)), your device is a \"${product}\""
  exit 1
fi

echo "This will restore your GPT and install Amazon Recovery."
echo "Press Enter to Continue..."
read

get_root

set +e
echo "Looking for partition-suffix"
adb shell su -c \"ls -l /dev/block/platform/soc/by-name\" | grep recovery_tmp
if [ $? -ne 0 ] ; then
  adb shell su -c \"ls -l /dev/block/platform/soc/by-name\" | grep recovery_x
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

echo "Flashing Amazon Recovery"
adb push bin/recovery.img /data/local/tmp/
adb shell su -c \"dd if=/data/local/tmp/recovery.img of=/dev/block/platform/soc/by-name/recovery${suffix_b} bs=512\" 
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

echo "Rebooting into Amazon Recovery"
adb reboot recovery
