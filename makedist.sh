#!/bin/bash

rm -rf dist

mkdir -p dist/unlock/amonet/bin
cp bin/{busybox,preloader.hdr0,preloader.hdr1,preloader.bin,lk.bin,tz.img,twrp.img,boot.hdr,boot.payload} dist/unlock/amonet/bin/
echo -ne "boot-recovery\x00" > dist/unlock/amonet/bin/boot-recovery.bin
dd if=/dev/zero bs=1 count=196624 >> dist/unlock/amonet/bin/boot-recovery.bin
echo -ne "WIPE_DATA" | dd of=dist/unlock/amonet/bin/boot-recovery.bin bs=1 seek=196608 conv=notrunc

mkdir -p dist/unlock/amonet/modules
cp modules/{common.py,gpt.py,handshake.py,handshake2.py,load_payload.py,logger.py,main.py} dist/unlock/amonet/modules/

mkdir -p dist/unlock/amonet/brom-payload/build
cp brom-payload/build/payload.bin dist/unlock/amonet/brom-payload/build/

cp {functions.inc,bootrom-step.sh,fastboot-step.sh,boot-fastboot.sh,boot-recovery.sh} dist/unlock/amonet/

mkdir -p dist/unlock/META-INF/com/google/android
cp META-INF/com/google/android/{update-binary,updater-script} dist/unlock/META-INF/com/google/android/

mkdir -p dist/stock/amonet/bin
cp bin/recovery.img dist/stock/amonet/bin/
cp return-to-stock.sh dist/stock/amonet/

mkdir -p dist/gptfix/amonet/bin
cp gpt-fix.sh dist/gptfix/amonet/
cp bin/gpt-rook.bin dist/gptfix/amonet/bin/

mkdir -p dist/full
cp -r dist/unlock/* dist/full/
cp -r dist/stock/* dist/full/
cp -r dist/gptfix/* dist/full/
