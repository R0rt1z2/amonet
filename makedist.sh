#!/bin/bash

set -e

VERSION=2.0.0

DEVICE=rook
BIN=bin/${DEVICE}
ZIP_NAME=amonet-${DEVICE}-v${VERSION}.zip

for f in preloader.img lk.bin tz.img twrp.img tee-payload.bin ${DEVICE}-kaeru.bin gpt-${DEVICE}.bin; do
  if [ ! -f ${BIN}/$f ]; then
    echo "error: ${BIN}/$f is missing, refusing to build $ZIP_NAME" >&2
    exit 1
  fi
done

for f in brom-payload/build/payload.bin brom-payload/build/pl.bin; do
  if [ ! -f $f ]; then
    echo "error: $f is missing, build brom-payload first" >&2
    exit 1
  fi
done

if ! ls ${BIN}/fastbrick*.img >/dev/null 2>&1; then
  echo "error: no fastbrick image in ${BIN}, build it with amonet-fastbrick first" >&2
  exit 1
fi

for f in fastbrick.sh fastbrick.bat fastbrick.ps1 \
         fastbrick/${DEVICE}/profile.sh fastbrick/${DEVICE}/profile.ps1; do
  if [ ! -f $f ]; then
    echo "error: $f is missing, refusing to build $ZIP_NAME" >&2
    exit 1
  fi
done

if ! rm -rf dist 2>/dev/null; then
  echo "error: could not remove dist, try: sudo rm -rf dist" >&2
  exit 1
fi

mkdir -p dist/unlock/amonet/bin
cp ${BIN}/{preloader.img,lk.bin,tz.img,twrp.img,tee-payload.bin,${DEVICE}-kaeru.bin} dist/unlock/amonet/bin/
echo -ne "boot-recovery\x00" > dist/unlock/amonet/bin/boot-recovery.bin
dd if=/dev/zero bs=1 count=196624 status=none >> dist/unlock/amonet/bin/boot-recovery.bin
echo -ne "WIPE_DATA" | dd of=dist/unlock/amonet/bin/boot-recovery.bin bs=1 seek=196608 conv=notrunc status=none

mkdir -p dist/unlock/amonet/modules
cp modules/{common.py,gpt.py,handshake.py,handshake2.py,load_payload.py,logger.py,main.py} dist/unlock/amonet/modules/

mkdir -p dist/unlock/amonet/brom-payload/build
cp brom-payload/build/{payload.bin,pl.bin} dist/unlock/amonet/brom-payload/build/

cp {bootrom-step.sh,fastboot-step.sh,boot-fastboot.sh,boot-recovery.sh} dist/unlock/amonet/

if ls ${BIN}/brick-*.img >/dev/null 2>&1; then
  cp ${BIN}/brick-*.img dist/unlock/amonet/bin/
  cp bin/{fastboot,fastboot32} dist/unlock/amonet/bin/
  cp {functions.inc,brick.sh} dist/unlock/amonet/
fi

# one shared set of scripts, with the per device data in profile.{sh,ps1}
cp ${BIN}/fastbrick*.img dist/unlock/amonet/bin/
cp {fastbrick.sh,fastbrick.bat,fastbrick.ps1} dist/unlock/amonet/
cp fastbrick/${DEVICE}/{profile.sh,profile.ps1} dist/unlock/amonet/

set +e
cp bin/fastboot* dist/unlock/amonet/bin/ 2>/dev/null
cp bin/*.dll dist/unlock/amonet/bin/ 2>/dev/null
set -e

chmod +x dist/unlock/amonet/*.sh

mkdir -p dist/unlock/META-INF/com/google/android
cp META-INF/com/google/android/{update-binary,updater-script} dist/unlock/META-INF/com/google/android/

if [ -f ${BIN}/recovery.img ]; then
  mkdir -p dist/stock/amonet/bin
  cp ${BIN}/recovery.img dist/stock/amonet/bin/
  cp return-to-stock.sh dist/stock/amonet/
  chmod +x dist/stock/amonet/*.sh
fi

mkdir -p dist/gptfix/amonet/bin
cp gpt-fix.sh dist/gptfix/amonet/
cp ${BIN}/gpt-${DEVICE}.bin dist/gptfix/amonet/bin/
chmod +x dist/gptfix/amonet/*.sh

mkdir -p dist/full
cp -r dist/unlock/* dist/full/
[ -d dist/stock ] && cp -r dist/stock/* dist/full/
cp -r dist/gptfix/* dist/full/

find dist/full -name __pycache__ -type d -exec rm -rf {} + 2>/dev/null

for f in bin/preloader.img bin/lk.bin bin/tz.img bin/tee-payload.bin bin/${DEVICE}-kaeru.bin bin/twrp.img \
         fastbrick.sh fastbrick.bat fastbrick.ps1 profile.sh profile.ps1; do
  if [ ! -f dist/full/amonet/$f ]; then
    echo "error: dist/full/amonet/$f is missing, refusing to build $ZIP_NAME" >&2
    exit 1
  fi
done

if ! ls dist/full/amonet/bin/fastbrick*.img >/dev/null 2>&1; then
  echo "error: no fastbrick image in dist/full/amonet/bin, refusing to build $ZIP_NAME" >&2
  exit 1
fi

(cd dist/full && zip -qr ../${ZIP_NAME} amonet META-INF)

echo "  -> dist/${ZIP_NAME}"
