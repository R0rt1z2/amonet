#!/bin/bash

rm -rf dist

mkdir -p dist/unlock/amonet/bin
cp bin/{preloader.img,lk.bin,tz.img,twrp.img,microloader.bin} dist/unlock/amonet/bin/

mkdir -p dist/unlock/amonet/lk-payload/build
cp lk-payload/build/payload.bin dist/unlock/amonet/lk-payload/build/

mkdir -p dist/unlock/amonet/modules
cp modules/{common.py,handshake.py,handshake2.py,load_payload.py,logger.py,main.py} dist/unlock/amonet/modules/

mkdir -p dist/unlock/amonet/brom-payload/build
cp brom-payload/build/payload.bin dist/unlock/amonet/brom-payload/build/

cp {bootrom-step.sh,fastboot-step.sh,boot-fastboot.sh,boot-recovery.sh} dist/unlock/amonet/
chmod +x dist/unlock/amonet/*.sh

mkdir -p dist/unlock/META-INF/com/google/android
cp META-INF/com/google/android/{update-binary,updater-script} dist/unlock/META-INF/com/google/android/

mkdir -p dist/gptfix/amonet/bin
cp gpt-fix.sh dist/gptfix/amonet/
chmod +x dist/gptfix/amonet/gpt-fix.sh
cp bin/gpt-checkers.bin dist/gptfix/amonet/bin/

set +e
cp bin/fastboot* dist/unlock/amonet/bin/ 2>/dev/null || true
cp bin/*.dll dist/unlock/amonet/bin/ 2>/dev/null || true
cp bin/full*.img dist/unlock/amonet/bin/ 2>/dev/null || true
cp {fastbrick.sh,fastbrick.bat,fastbrick.ps1} dist/unlock/amonet/ 2>/dev/null || true
chmod +x dist/unlock/amonet/fastbrick.* 2>/dev/null || true
set -e

mkdir -p dist/full
cp -r dist/unlock/* dist/full/
