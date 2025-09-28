#!/bin/bash

set -e

fastboot flash tee2 bin/tz.img
fastboot flash recovery bin/twrp.img
fastboot oem reboot-recovery

echo ""
echo "The device should now boot into TWRP recovery, indicated by a constantly blinking cyan LED."
echo ""
echo "If you were previously on FireOS 6, consider flashing a FireOS 5-based ROM to avoid a potential boot loop."
echo ""