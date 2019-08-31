#!/bin/bash

set -e

echo "Your device will be reset to factory defaults..."
echo "Press Enter to Continue..."
read

fastboot flash recovery_x bin/twrp.img
fastboot erase userdata
#fastboot format userdata
fastboot flash MISC bin/boot-recovery.bin
fastboot reboot

echo ""
echo "Your device will now reboot into TWRP."
echo ""
