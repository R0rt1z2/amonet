#!/bin/bash

DEVICE=checkers
VERSION=2.0.0

ZIP_NAME=amonet-${DEVICE}-${VERSION}.zip

rm -rf dist

mkdir -p dist/unlock/amonet/bin
cp bin/{preloader.img,lk.bin,tz.img,tee-payload.bin,${DEVICE}-kaeru.bin,twrp.img} dist/unlock/amonet/bin/

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
cp bin/gpt-${DEVICE}.bin dist/gptfix/amonet/bin/

set +e
cp bin/fastboot* dist/unlock/amonet/bin/ 2>/dev/null || true
cp bin/*.dll dist/unlock/amonet/bin/ 2>/dev/null || true
cp bin/full*.img dist/unlock/amonet/bin/ 2>/dev/null || true
cp {fastbrick.sh,fastbrick.bat,fastbrick.ps1} dist/unlock/amonet/ 2>/dev/null || true
chmod +x dist/unlock/amonet/fastbrick.* 2>/dev/null || true
set -e

mkdir -p dist/full
cp -r dist/unlock/* dist/full/

for f in preloader.img lk.bin tz.img tee-payload.bin ${DEVICE}-kaeru.bin twrp.img; do
  if [ ! -f dist/full/amonet/bin/$f ]; then
    echo "error: dist/full/amonet/bin/$f is missing, refusing to build $ZIP_NAME" >&2
    exit 1
  fi
done

(cd dist/full && zip -qr ../${ZIP_NAME} amonet META-INF)
mv dist/${ZIP_NAME} dist/full/${ZIP_NAME}

echo "Created dist/full/${ZIP_NAME}"
