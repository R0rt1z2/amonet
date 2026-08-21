#!/bin/bash

set -e

cd "$(cd "$(dirname "$0")" && pwd)"

VERSION="1.2.0"
DEVICE="karat"

ZIP="amonet-${DEVICE}-v${VERSION}.zip"
DIST="dist/amonet"

rm -rf dist "$ZIP"

mkdir -p "$DIST/bin"
cp bin/{gpt-karat.bin,karat-kaeru.bin,mcupm.img,tz.img,twrp.img,unlock} "$DIST/bin/"

for f in adb-linux adb-darwin adb.exe AdbWinApi.dll AdbWinUsbApi.dll libwinpthread-1.dll; do
    if [ -f "bin/$f" ]; then
        cp "bin/$f" "$DIST/bin/"
    else
        echo "warning: bin/$f not found, not bundled"
    fi
done

mkdir -p "$DIST/modules"
cp modules/{common.py,functions.py,gpt.py,handshake2.py,load_payload.py,logger.py,main.py} "$DIST/modules/"

mkdir -p "$DIST/pl-payload/pl"
cp pl-payload/pl/pl.bin "$DIST/pl-payload/pl/"

cp bootrom-step.sh fastboot-step.sh boot-recovery.sh boot-fastboot.sh gpt-fix.sh "$DIST/"
cp unlock.sh unlock.ps1 unlock.bat "$DIST/"
cp boot-recovery.bat boot-fastboot.bat "$DIST/"
cp README.md requirements.txt "$DIST/"


mkdir -p "dist/META-INF/com/google/android"
cp META-INF/com/google/android/{update-binary,updater-script} "dist/META-INF/com/google/android/"
chmod 755 "dist/META-INF/com/google/android/update-binary"

chmod 755 "$DIST"/*.sh "$DIST/bin/unlock"

for f in bin/twrp.img bin/karat-kaeru.bin; do
    if [ ! -f "$DIST/$f" ]; then
        echo "error: $DIST/$f is missing, refusing to build $ZIP" >&2
        exit 1
    fi
done

cd dist
zip -qr "../$ZIP" amonet META-INF
cd ..

echo "$ZIP"
