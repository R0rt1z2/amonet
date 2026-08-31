#!/bin/bash

set -e

cd "$(cd "$(dirname "$0")" && pwd)"

VERSION="1.2.0"
DEVICE="sunstone"

ZIP="amonet-${DEVICE}-v${VERSION}.zip"
DIST="dist/amonet"

rm -rf dist "$ZIP"

BIN_FILES="gpt-sunstone.bin sunstone-kaeru.bin mcupm.img spmfw.img sspm.img dpm.img dtbo.img apusys.img cam_vpu1.img cam_vpu2.img tz.img twrp.img unlock"

mkdir -p "$DIST/bin"
for f in $BIN_FILES; do
    if [ ! -f "bin/$f" ]; then
        echo "error: bin/$f is missing, refusing to build $ZIP" >&2
        exit 1
    fi
    cp "bin/$f" "$DIST/bin/"
done

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

cd dist
zip -qr "../$ZIP" amonet META-INF
cd ..

echo "$ZIP"
