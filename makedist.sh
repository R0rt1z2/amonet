#!/bin/bash

rm -rf dist

if [ ! -f bin/preloader.bin ] || [ ! -f bin/tz.img ] || [ ! -f bin/lk.bin ] || [ ! -f bin/twrp.img ] ; then
	echo "Missing binary files in bin/"
	exit 1
fi

mkdir -p dist/unlock/amonet/bin
cp bin/{busybox,preloader.bin,lk.bin,tz.img,twrp.img,boot.hdr,boot.payload} dist/unlock/amonet/bin/

echo -ne "boot-recovery\x00" > dist/unlock/amonet/bin/boot-recovery.bin
dd if=/dev/zero bs=1 count=196624 >> dist/unlock/amonet/bin/boot-recovery.bin
echo -ne "WIPE_DATA" | dd of=dist/unlock/amonet/bin/boot-recovery.bin bs=1 seek=196608 conv=notrunc

mkdir -p dist/unlock/amonet/modules
cp modules/{common.py,gpt.py,handshake.py,handshake2.py,logger.py,main.py,mmc.py,functions.py} dist/unlock/amonet/modules/
cp {functions.inc,bootrom-step.sh,fastboot-step.sh,boot-fastboot.sh,boot-recovery.sh} dist/unlock/amonet/

mkdir -p dist/unlock/META-INF/com/google/android
cp META-INF/com/google/android/{update-binary,updater-script} dist/unlock/META-INF/com/google/android/

mkdir -p dist/stock/amonet/bin
cp bin/recovery.img dist/stock/amonet/bin/
cp bin/tz.img dist/stock/amonet/bin/
cp {return-to-stock.sh,functions.inc} dist/stock/amonet/

mkdir -p dist/gptfix/amonet/bin
cp {gpt-fix-16G.sh,gpt-fix-8G.sh} dist/gptfix/amonet/
cp gpt/gpt-thempis-8G.bin dist/gptfix/amonet/bin/

mkdir -p dist/full
cp -r dist/unlock/* dist/full/
cp -r dist/gptfix/* dist/full/
