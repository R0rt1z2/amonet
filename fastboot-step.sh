#!/bin/bash

set -e

echo "Your device will be reset to factory defaults..."
echo "Press Enter to Continue..."
read

fastboot flash recovery_x bin/twrp.img
fastboot erase userdata
#fastboot format userdata

echo ""
echo "Hold the left volume-button, then press Enter to reboot..."
read
fastboot reboot
echo "Rebooting... keep holding the button until you see TWRP"
echo ""
