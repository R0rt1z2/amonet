#!/bin/bash

set -e

fastboot flash tee2 bin/tz.img
fastboot flash recovery bin/twrp.img
fastboot reboot recovery

echo ""
echo ""
echo "If you don't see the recovery in a few seconds, try pressing the power button twice"
echo ""
