#!/bin/bash

rm -rf dist

mkdir -p dist/unlock/amonet/bin
cp bin/{fastboot,fastboot32,brick-*.img,preloader.img,lk.bin,tz.img,twrp.img,boot.hdr,boot.payload} dist/unlock/amonet/bin/

mkdir -p dist/unlock/amonet/modules
cp modules/{common.py,gpt.py,handshake.py,handshake2.py,load_payload.py,logger.py,main.py} dist/unlock/amonet/modules/

mkdir -p dist/unlock/amonet/brom-payload/build
cp brom-payload/build/payload.bin dist/unlock/amonet/brom-payload/build/

cp {brick.sh,bootrom-step.sh,fastboot-step.sh,boot-fastboot.sh,boot-recovery.sh} dist/unlock/amonet/

# mkdir -p dist/unlock/META-INF/com/google/android
# cp META-INF/com/google/android/{update-binary,updater-script} dist/unlock/META-INF/com/google/android/

mkdir -p dist/gptfix/amonet/bin
cp gpt-fix.sh dist/gptfix/amonet/
cp bin/gpt-biscuit.bin dist/gptfix/amonet/bin/

mkdir -p dist/full
cp -r dist/unlock/* dist/full/
cp -r dist/gptfix/* dist/full/