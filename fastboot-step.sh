#!/bin/bash

set -e

echo "Press Enter to Continue..."
read

fastboot flash TEE2 bin/tz.img
fastboot flash TEE1 bin/tz.img
fastboot flash recovery_x bin/twrp.img
fastboot flash MISC bin/boot-recovery.bin
fastboot reboot

echo ""
echo ""
echo "Your device should now reboot into TWRP"
echo ""
