#!/bin/bash

set -e

VERSION=1.0.0

DEVICE=radar
ZIP_NAME=amonet-${DEVICE}-v${VERSION}.zip

for f in preloader.img lk.bin tz.img tee-payload.bin ${DEVICE}-kaeru.bin twrp.img; do
  if [ ! -s bin/$f ]; then
    echo "error: bin/$f is missing, refusing to build $ZIP_NAME" >&2
    exit 1
  fi
done

for f in brom-payload/build/payload.bin brom-payload/build/pl.bin; do
  if [ ! -s $f ]; then
    echo "error: $f is missing, build brom-payload first" >&2
    exit 1
  fi
done

rm -rf dist

mkdir -p dist/unlock/amonet/bin
cp bin/{preloader.img,lk.bin,tz.img,tee-payload.bin,${DEVICE}-kaeru.bin,twrp.img} dist/unlock/amonet/bin/

mkdir -p dist/unlock/amonet/modules
cp modules/{common.py,gpt.py,handshake.py,handshake2.py,load_payload.py,logger.py,main.py} dist/unlock/amonet/modules/

mkdir -p dist/unlock/amonet/brom-payload/build
cp brom-payload/build/{payload.bin,pl.bin} dist/unlock/amonet/brom-payload/build/

cp {bootrom-step.sh,fastboot-step.sh,boot-fastboot.sh,boot-recovery.sh} dist/unlock/amonet/

if [ -s bin/fastboot ] && [ -s bin/fastboot32 ] && [ -s bin/fastboot.exe ]; then
  cp bin/{fastboot,fastboot32,fastboot.exe} dist/unlock/amonet/bin/
  cp bin/*.dll dist/unlock/amonet/bin/ 2>/dev/null || true
  chmod +x dist/unlock/amonet/bin/{fastboot,fastboot32}
else
  echo "note: no fastboot binaries in bin/, the scripts will need one on PATH"
fi

if ls bin/fastbrick*.img >/dev/null 2>&1; then
  cp bin/fastbrick*.img dist/unlock/amonet/bin/
  cp {fastbrick.sh,fastbrick.bat,fastbrick.ps1} dist/unlock/amonet/
  cp fastbrick/${DEVICE}/{profile.sh,profile.ps1} dist/unlock/amonet/
else
  echo "note: no fastbrick image in bin/, leaving the fastbrick scripts out"
fi

if ls bin/brick-*.img >/dev/null 2>&1; then
  cp bin/brick-*.img dist/unlock/amonet/bin/
  cp brick.sh dist/unlock/amonet/
else
  echo "note: no brick image in bin/, leaving brick.sh out"
fi

chmod +x dist/unlock/amonet/*.sh

mkdir -p dist/unlock/META-INF/com/google/android
cp META-INF/com/google/android/{update-binary,updater-script} dist/unlock/META-INF/com/google/android/
chmod +x dist/unlock/META-INF/com/google/android/update-binary

if [ -s bin/gpt-${DEVICE}.bin ]; then
  mkdir -p dist/gptfix/amonet/bin
  cp gpt-fix.sh dist/gptfix/amonet/
  cp bin/gpt-${DEVICE}.bin dist/gptfix/amonet/bin/
  chmod +x dist/gptfix/amonet/*.sh
else
  echo "note: no bin/gpt-${DEVICE}.bin, leaving the gptfix package out"
fi

mkdir -p dist/full
cp -r dist/unlock/* dist/full/
[ -d dist/gptfix ] && cp -r dist/gptfix/* dist/full/

find dist -name __pycache__ -type d -exec rm -rf {} + 2>/dev/null || true

for f in bin/preloader.img bin/lk.bin bin/tz.img bin/tee-payload.bin bin/${DEVICE}-kaeru.bin bin/twrp.img \
         modules/main.py brom-payload/build/payload.bin brom-payload/build/pl.bin bootrom-step.sh; do
  if [ ! -s dist/full/amonet/$f ]; then
    echo "error: dist/full/amonet/$f is missing, refusing to build $ZIP_NAME" >&2
    exit 1
  fi
done

if [ ! -s dist/full/META-INF/com/google/android/update-binary ]; then
  echo "error: the updater is missing from dist/full, refusing to build $ZIP_NAME" >&2
  exit 1
fi

(cd dist/full && zip -qr ../${ZIP_NAME} amonet META-INF)

echo ""
echo "  -> dist/${ZIP_NAME}"
