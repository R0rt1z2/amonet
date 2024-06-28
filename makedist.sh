#!/bin/bash

rm -rf dist

if [ ! -f bin/preloader.bin ] || [ ! -f bin/tz.img ] || [ ! -f bin/lk.bin ] || [ ! -f bin/twrp.img ] ; then
	echo "Missing binary files in bin/"
	exit 1
fi

mkdir -p dist/unlock/amonet/bin
cp bin/{preloader.bin,lk.bin,tz.img,twrp.img,boot.hdr,boot.payload} dist/unlock/amonet/bin/
cp gpt/gpt-ariel-amonet.bin dist/unlock/amonet/bin/

echo -ne "boot-recovery\x00" > dist/unlock/amonet/bin/boot-recovery.bin

mkdir -p dist/unlock/amonet/modules
cp modules/{common.py,gpt.py,handshake.py,handshake2.py,logger.py,main.py,mmc.py,functions.py} dist/unlock/amonet/modules/

cp {bootrom-step.sh,fastboot-step.sh,boot-fastboot.sh,boot-recovery.sh} dist/unlock/amonet/

mkdir -p dist/unlock/META-INF/com/google/android
cp META-INF/com/google/android/{update-binary,updater-script} dist/unlock/META-INF/com/google/android/

mkdir -p dist/gptfix/amonet/bin
cp gpt-fix.sh dist/gptfix/amonet/
cp gpt/gpt-ariel.bin dist/gptfix/amonet/bin/

mkdir -p dist/full
cp -r dist/unlock/* dist/full/
cp -r dist/gptfix/* dist/full/
