#!/bin/bash

set -e

if [ `uname -m | grep 64` ]; then
	FASTBOOT=bin/fastboot
else
	FASTBOOT=bin/fastboot32
fi

detect_image() {   
    LK_BUILD_DESC=$($FASTBOOT getvar lk_build_desc 2>&1 | grep "lk_build_desc:" | cut -d' ' -f2-)
    
    if [ -z "$LK_BUILD_DESC" ]; then
        echo "Error: Could not retrieve lk_build_desc from device"
        echo "Make sure device is in fastboot mode and connected"
        exit 1
    fi
    
    case "$LK_BUILD_DESC" in
        "c1c79aa-20220824_171625")
            BRICK_IMAGE="bin/brick-690916520.img"
            VERSION_DESC="FireOS 5.5.6.9"
            ;;
        "beb022a-20211202_232416")
            BRICK_IMAGE="bin/brick-677719420.img"
            VERSION_DESC="FireOS 5.5.5.2"
            ;;
        "d9a2246-20180702_202914")
            BRICK_IMAGE="bin/brick-647591020.img"
            VERSION_DESC="FireOS 5.5.3.4"
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
  echo "Bricking PL Header, check instructions on device!"
  echo "Fastboot will HANG, once you see instructions, disconnect and continue with bootrom-step.sh"
  echo ""
  $FASTBOOT flash brick "$BRICK_IMAGE"
fi