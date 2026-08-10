#!/bin/bash

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

if [ ! -f "$SCRIPT_DIR/profile.sh" ]; then
    echo "Error: profile.sh not found next to this script."
    echo "The package is incomplete - re-extract it and try again."
    exit 1
fi

. "$SCRIPT_DIR/profile.sh"

if [ `uname -m | grep 64` ]; then
    FASTBOOT=bin/fastboot
else
    FASTBOOT=bin/fastboot32
fi

chmod a+x bin/fastboot{,32}

declare -A DEVICE_MAP
DEVICE_MAP["CRONOS"]="Echo Show 5 2nd Generation - 2021"
DEVICE_MAP["CROWN"]="Echo Show 8 1st Generation - 2019"
DEVICE_MAP["CHECKERS"]="Echo Show 5 1st Generation - 2019"

if [ -t 1 ] && command -v tput >/dev/null 2>&1 && tput colors >/dev/null 2>&1 && [ $(tput colors) -ge 8 ]; then
    RED='\033[31m'
    GREEN='\033[32m'
    YELLOW='\033[33m'
    BLUE='\033[34m'
    MAGENTA='\033[35m'
    CYAN='\033[36m'
    RESET='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    MAGENTA=''
    CYAN=''
    RESET=''
fi

print_banner() {
    echo -e "${MAGENTA}                                   _   "
    echo -e "                                  | |  "
    echo -e "   __ _ _ __ ___   ___  _ __   ___| |_ "
    echo -e "  / _' | '_ ' _ \\ / _ \\| '_ \\ / _ \\ __|"
    echo -e " | (_| | | | | | | (_) | | | |  __/ |_  (-fastbrick)"
    echo -e "  \\__,_|_| |_| |_|\\___/|_| |_|\\___|\\_|"
    echo -e "            by k4y0z & r0rt1z2         "
    echo -e "                                       ${RESET}"
}

# The payload itself rejects the wrong device ("Device mismatch"), so this only
# waits for something to show up - without it the first getvar blocks silently.
wait_for_device() {
    echo -e "${YELLOW}Make sure device is in fastboot mode and connected!${RESET}"
    echo ""
    echo -e "${YELLOW}To enter fastboot mode:${RESET}"
    echo -e "${YELLOW}  1. Power off the device${RESET}"
    echo -e "${YELLOW}  2. Hold Volume Down + Volume Up + Mute buttons simultaneously${RESET}"
    echo -e "${YELLOW}  3. Keep holding until the fastboot screen appears${RESET}"
    echo ""
    echo -e "${YELLOW}If the device is already in fastboot mode, just connect it via USB.${RESET}"
    echo ""

    while true; do
        OUTPUT=$($FASTBOOT getvar product 2>&1)

        if echo "$OUTPUT" | grep -q "waiting for any device"; then
            sleep 1
            continue
        fi

        PRODUCT=$(echo "$OUTPUT" | grep "product:" | cut -d' ' -f2)

        if [ -z "$PRODUCT" ]; then
            sleep 1
            continue
        fi

        FRIENDLY_NAME="${DEVICE_MAP[$PRODUCT]}"
        if [ -z "$FRIENDLY_NAME" ]; then
            FRIENDLY_NAME="Unknown device"
        fi

        echo -e "${CYAN}Detected: $PRODUCT - $FRIENDLY_NAME${RESET}"
        break
    done
}

check_unlocked() {
    OUTPUT=$($FASTBOOT getvar unlock_status 2>&1)
    UNLOCK_STATUS=$(echo "$OUTPUT" | grep "unlock_status:" | cut -d' ' -f2 | head -n1 | tr '[:upper:]' '[:lower:]')

    if [ "$UNLOCK_STATUS" = "true" ]; then
        echo ""
        echo -e "${GREEN}Your bootloader is already unlocked!${RESET}"
        echo ""
        echo -e "${GREEN}The amonet-fastbrick exploit cannot be run on a device that is${RESET}"
        echo -e "${GREEN}already unlocked. There is no need to run it again.${RESET}"
        echo ""
        echo "Press any key to exit..."
        read -n 1 -s
        exit 0
    fi
}

detect_image() {
    FULL_IMAGE="$DEFAULT_IMAGE"

    OUTPUT=$($FASTBOOT getvar lk_build_desc 2>&1)
    LK_BUILD_DESC=$(echo "$OUTPUT" | grep "lk_build_desc:" | cut -d' ' -f2- | head -n1)

    if [ -n "$LK_BUILD_DESC" ]; then
        echo -e "${CYAN}LK build: $LK_BUILD_DESC${RESET}"
    fi

    if [ -n "$LK_BUILD_DESC" ]; then
        for build in "${!IMAGE_MAP[@]}"; do
            case "$LK_BUILD_DESC" in
                "$build"*)
                    FULL_IMAGE="${IMAGE_MAP[$build]}"
                    break
                    ;;
            esac
        done
    fi

    echo -e "${CYAN}Will use payload: $(basename "$FULL_IMAGE")${RESET}"

    if [ ! -f "$FULL_IMAGE" ]; then
        echo -e "${RED}Error: Payload file not found: $FULL_IMAGE${RESET}"
        exit 1
    fi
}

flash_payload() {
    echo -e "${YELLOW}Sending payload, this might take a few attempts...${RESET}"
    local attempt=1
    while true; do
        set +e
        OUTPUT=$(timeout 8 $FASTBOOT flash brick "$FULL_IMAGE" 2>&1)
        EXIT_CODE=$?
        set -e

        if echo "$OUTPUT" | grep -q "eMMC-RO"; then
            echo ""
            echo -e "${RED}eMMC is in permanent read-only mode!${RESET}"
            echo ""
            echo -e "${RED}This device's storage has reached the end of its life and has${RESET}"
            echo -e "${RED}locked itself read-only. Writes report success but never persist,${RESET}"
            echo -e "${RED}so the bootloader cannot be unlocked and nothing can be flashed.${RESET}"
            echo ""

            BOOTS=$(echo "$OUTPUT" | sed -n 's/.*boots=\([0-9][0-9]*\).*/\1/p' | head -n1)
            if [ -n "$BOOTS" ]; then
                echo -e "${YELLOW}For reference, this device has booted $BOOTS times.${RESET}"
                if [ "$BOOTS" -lt 100 ]; then
                    echo -e "${YELLOW}Damn... only $BOOTS boots and the flash already gave out.${RESET}"
                    echo -e "${YELLOW}You must've been seriously unlucky with this one.${RESET}"
                fi
                echo ""
            fi

            echo -e "${RED}This is a hardware failure - the eMMC chip would need to be${RESET}"
            echo -e "${RED}physically replaced. The device was not modified and is safe to${RESET}"
            echo -e "${RED}reboot.${RESET}"
            exit 1
        fi

        if echo "$OUTPUT" | grep -q "Device mismatch"; then
            echo ""
            echo -e "${RED}Device mismatch detected!${RESET}"
            echo ""
            echo -e "${RED}The payload is not compatible with your device. You may be running${RESET}"
            echo -e "${RED}this script on an unsupported device or with the wrong payload.${RESET}"
            echo ""
            echo -e "${RED}Please verify your device and try again with the correct payload.${RESET}"
            exit 1
        fi

        if [ $EXIT_CODE -eq 124 ]; then
            echo -e "${GREEN}Exploit most likely successful!${RESET}"
            return 0
        fi

        sleep 2
        attempt=$((attempt + 1))
    done
}

clear
print_banner

wait_for_device
check_unlocked
detect_image

echo ""
echo -n "Run amonet-fastbrick to unlock the bootloader? (Type \"YES\" to continue): "
read YES
if [ "$YES" = "YES" ]; then
    echo ""
    flash_payload
    echo ""
    echo -e "${YELLOW}Please wait until the process finishes and DO NOT INTERRUPT IT.${RESET}"
else
    echo ""
    echo -e "${YELLOW}Aborting.${RESET}"
    exit 0
fi

echo ""
echo "Press any key to exit..."
read -n 1 -s
