#!/bin/bash

set -e

fastboot flash recovery_x bin/twrp.img
fastboot erase userdata
#fastboot format userdata
fastboot oem reboot-recovery

echo ""
echo ""
echo "Your device should now restart into TWRP"
echo "You should first do a factory-reset (otherwise it should happen automatically when you reboot)"
echo ""
