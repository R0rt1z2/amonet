#!/bin/bash

set -e

cd "$(cd "$(dirname "$0")" && pwd)"

VERSION="1.0.0"
NAME="amonet-karat"
ZIP="$NAME-v$VERSION.zip"
DIST="dist/$NAME"

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

chmod 755 "$DIST"/*.sh "$DIST/bin/unlock"

cd dist
zip -qr "../$ZIP" "$NAME"
cd ..

echo "$ZIP"
