#!/bin/bash

rm -rf dist

mkdir -p dist/unlock/amonet/bin
cp bin/{boot0short.img,preloader.bin,minisu.img,busybox,lk.bin,tz.img,twrp.img,boot.hdr,boot.payload} dist/unlock/amonet/bin/

mkdir -p dist/unlock/amonet/modules
cp modules/{common.py,gpt.py,handshake.py,handshake2.py,load_payload.py,logger.py,main.py} dist/unlock/amonet/modules/

mkdir -p dist/unlock/amonet/brom-payload/build
cp brom-payload/build/payload.bin dist/unlock/amonet/brom-payload/build/

cp {functions.inc,step-1.sh,step-2.sh,bootrom-step.sh,bootrom-step-minimal.sh,fastboot-step.sh,boot-fastboot.sh,boot-recovery.sh} dist/unlock/amonet/

mkdir -p dist/unlock/META-INF/com/google/android
cp META-INF/com/google/android/{update-binary,updater-script} dist/unlock/META-INF/com/google/android/

mkdir -p dist/stock/amonet/bin
cp bin/recovery.img dist/stock/amonet/bin/
cp return-to-stock.sh dist/stock/amonet/

mkdir -p dist/full
cp -r dist/unlock/* dist/full/
cp -r dist/stock/* dist/full/
