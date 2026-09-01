#!/bin/bash

set -e

cd "$(cd "$(dirname "$0")" && pwd)"

fastboot flash recovery bin/twrp.img
fastboot reboot recovery

echo ""
echo "Your device will now reboot into TWRP."
echo ""
