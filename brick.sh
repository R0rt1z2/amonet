#!/bin/bash

set -e

if [ `uname -m | grep 64` ]; then
	FASTBOOT=bin/fastboot
else
	FASTBOOT=bin/fastboot32
fi

chmod a+x bin/fastboot{,32}

detect_image() {   
    LK_BUILD_DESC=$($FASTBOOT getvar lk_build_desc 2>&1 | grep "lk_build_desc:" | cut -d' ' -f2-)
    
    if [ -z "$LK_BUILD_DESC" ]; then
        echo "Error: Could not retrieve lk_build_desc from device"
        echo "Make sure device is in fastboot mode and connected"
        exit 1
    fi
    
    case "$LK_BUILD_DESC" in
        "63cb91b-20221007_072309")
            BRICK_IMAGE="bin/brick-NS6570.img"
            VERSION_DESC="Fire OS 6.5.7.0 (NS6570/6071)"
            ;;
        *)
            echo "Error: Unsupported version: $LK_BUILD_DESC"
            echo "Please update to one of the supported versions or contact the developer"
            exit 1
            ;;
    esac
    
    echo "Detected $VERSION_DESC"
    echo "Will use brick image: $BRICK_IMAGE"
    
    if [ ! -f "$BRICK_IMAGE" ]; then
        echo "Error: Brick image file not found: $BRICK_IMAGE"
        exit 1
    fi
}

detect_image

echo ""
echo "Brick preloader to continue via bootrom-exploit? (Type \"YES\" to continue)"
read YES
if [ "$YES" = "YES" ]; then
    echo ""
    echo "Bricking PL Header, check LEDs on the device!"
    echo ""
    $FASTBOOT flash brick "$BRICK_IMAGE" > /dev/null 2>&1 || true
    timeout 5 $FASTBOOT flash brick "$BRICK_IMAGE" > /dev/null 2>&1 || true
    echo "If the device did not show a rainbow LED ring, try again"
    echo "If it did, disconnect the device and run bootrom-step.sh"
fi
