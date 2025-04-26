#!/bin/bash

. functions.inc

echo "Waiting for device to be connected..."
adb wait-for-device

product=$(adb shell getprop ro.product.name | dos2unix)

if [[ "$product" != *"thebes"* && "$product" != *"memphis"* && "$product" != *"thempis"* ]]; then
    echo "This script is only for devices containing the name 'thebes', 'memphis' or 'thempis' (e.g., Amazon Fire HD8 / HD10 (2015)), your device is a \"${product}\"."
    exit 1
fi

check_root

echo ""
echo "This will restore your GPT and install stock recovery."
echo "Press Enter to Continue..."
read

set +e
echo "Looking for partition-suffix"
adb shell ${CMD_PREFIX}ls -l /dev/block/platform/mtk-msdc.0/by-name${CMD_SUFFIX} | grep recovery_tmp
if [ $? -ne 0 ] ; then
  adb shell ${CMD_PREFIX}ls -l /dev/block/platform/mtk-msdc.0/by-name${CMD_SUFFIX} | grep recovery_x
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
adb shell ${CMD_PREFIX}dd if=/dev/block/mmcblk0 bs=512 count=34 of=/data/local/tmp/gpt.bin${CMD_SUFFIX}
adb shell ${CMD_PREFIX}chmod 644 /data/local/tmp/gpt.bin${CMD_SUFFIX}
adb pull /data/local/tmp/gpt.bin gpt-regen/gpt.bin
echo ""

echo "Unpatching GPT"
modules/gpt.py unpatch gpt-regen/gpt.bin
[ ! -d gpt ] && mkdir gpt
cp gpt-regen/gpt.bin.unpatched.gpt gpt/gpt.bin
cp gpt-regen/gpt.bin.unpatched.bak gpt/gpt.bin.bak
cp gpt-regen/gpt.bin.offset gpt/gpt.bin.offset
echo ""

echo "Flashing stock recovery"
adb push bin/recovery.img /data/local/tmp/
adb shell ${CMD_PREFIX}dd if=/data/local/tmp/recovery.img of=/dev/block/platform/mtk-msdc.0/by-name/recovery${suffix_b} bs=512${CMD_SUFFIX}
echo ""

echo "Flashing old preloader"
adb push bin/tz.img /data/local/tmp/
adb shell ${CMD_PREFIX}dd if=/data/local/tmp/tz.img of=/dev/block/platform/mtk-msdc.0/by-name/TEE1 bs=512${CMD_SUFFIX} # Better safe than sorry
adb shell ${CMD_PREFIX}dd if=/data/local/tmp/tz.img of=/dev/block/platform/mtk-msdc.0/by-name/TEE1_real bs=512${CMD_SUFFIX} 2>/dev/null
adb shell ${CMD_PREFIX}dd if=/data/local/tmp/tz.img of=/dev/block/platform/mtk-msdc.0/by-name/TEE2 bs=512${CMD_SUFFIX} # Better safe than sorry
adb shell ${CMD_PREFIX}dd if=/data/local/tmp/tz.img of=/dev/block/platform/mtk-msdc.0/by-name/TEE2_real bs=512${CMD_SUFFIX} 2>/dev/null
echo ""

echo "Flashing unpatched GPT"
adb push gpt/gpt.bin /data/local/tmp/
adb shell ${CMD_PREFIX}dd if=/data/local/tmp/gpt.bin of=/dev/block/mmcblk0 bs=512 count=34${CMD_SUFFIX}
echo ""
if [ -f "gpt/gpt.bin.offset" ] ; then
  OFFSET=$(cat gpt/gpt.bin.offset)
  # Check if $OFFSET has some sane value
  if [ $OFFSET -gt 25000000 ] ; then
    echo "Flashing unpatched GPT (backup)"
    adb push gpt/gpt.bin.bak /data/local/tmp/
    adb shell ${CMD_PREFIX}dd if=/data/local/tmp/gpt.bin.bak of=/dev/block/mmcblk0 bs=512 seek=${OFFSET}${CMD_SUFFIX}
    echo ""
  fi
fi

echo "To complete the process and have a bootable system,"
echo "you MUST flash a full stock update.bin manually. If"
echo "you skip this step, your device will NOT boot."
echo ""

echo "Rebooting to stock recovery"
adb reboot recovery