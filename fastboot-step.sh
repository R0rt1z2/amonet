#!/bin/bash

set -e

echo "Press Enter to Continue..."
read

fastboot flash recovery bin/twrp.img
fastboot reboot recovery

echo ""
echo "Your device will now reboot into TWRP."
echo ""
