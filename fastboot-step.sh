#!/bin/bash

set -e

fastboot flash recovery bin/twrp.img
fastboot reboot recovery

echo ""
echo "Your device will now reboot into TWRP."
echo ""
