#!/bin/bash

set -e

VERSION=2.0.0

device_desc() {
  case "$1" in
    checkers) echo " - Amazon Echo Show 5 (2019 / 1st Gen) - " ;;
    crown)    echo " - Amazon Echo Show 8 (2019 / 1st Gen) - " ;;
    cronos)   echo " - Amazon Echo Show 5 (2021 / 2nd Gen) - " ;;
    *) return 1 ;;
  esac
}

AVAILABLE=()
for d in bin/*/; do
  [ -d "$d" ] || continue
  AVAILABLE+=("$(basename "$d")")
done

if [ ${#AVAILABLE[@]} -eq 0 ]; then
  echo "error: no device directories found in bin/" >&2
  exit 1
fi

if [ $# -eq 0 ]; then
  DEVICES=("${AVAILABLE[@]}")
else
  DEVICES=("$@")
  for DEVICE in "${DEVICES[@]}"; do
    if [ ! -d "bin/${DEVICE}" ]; then
      echo "error: unknown device '${DEVICE}'" >&2
      echo "available: ${AVAILABLE[*]}" >&2
      exit 1
    fi
  done
fi

for DEVICE in "${DEVICES[@]}"; do
  if ! device_desc "${DEVICE}" >/dev/null; then
    echo "error: unregistered device '${DEVICE}', add it to device_desc in makedist.sh" >&2
    exit 1
  fi
done

echo "building: ${DEVICES[*]}"

rm -rf dist

for DEVICE in "${DEVICES[@]}"; do
  DEVICE_DESC=$(device_desc "${DEVICE}")

  ZIP_NAME=amonet-${DEVICE}-v${VERSION}.zip
  OUT=dist/${DEVICE}

  mkdir -p ${OUT}/unlock/amonet/bin
  cp bin/${DEVICE}/{preloader.img,lk.bin,tz.img,tee-payload.bin,${DEVICE}-kaeru.bin,twrp.img} ${OUT}/unlock/amonet/bin/

  mkdir -p ${OUT}/unlock/amonet/modules
  cp modules/{common.py,handshake.py,handshake2.py,load_payload.py,logger.py,main.py} ${OUT}/unlock/amonet/modules/

  mkdir -p ${OUT}/unlock/amonet/brom-payload/build
  cp brom-payload/build/payload.bin ${OUT}/unlock/amonet/brom-payload/build/

  cp {bootrom-step.sh,fastboot-step.sh,boot-fastboot.sh,boot-recovery.sh} ${OUT}/unlock/amonet/
  chmod +x ${OUT}/unlock/amonet/*.sh

  mkdir -p ${OUT}/unlock/META-INF/com/google/android
  cp META-INF/com/google/android/{update-binary,updater-script} ${OUT}/unlock/META-INF/com/google/android/

  printf 'DEVICE=%s\nDEVICE_DESC="%s"\n' "${DEVICE}" "${DEVICE_DESC}" > ${OUT}/unlock/amonet/device.prop

  mkdir -p ${OUT}/gptfix/amonet/bin
  cp gpt-fix.sh ${OUT}/gptfix/amonet/
  chmod +x ${OUT}/gptfix/amonet/gpt-fix.sh
  cp bin/${DEVICE}/gpt-${DEVICE}.bin ${OUT}/gptfix/amonet/bin/

  # one shared set of scripts, with the per device data in profile.{sh,ps1}
  cp bin/${DEVICE}/fastbrick*.img ${OUT}/unlock/amonet/bin/
  cp {fastbrick.sh,fastbrick.bat,fastbrick.ps1} ${OUT}/unlock/amonet/
  cp fastbrick/${DEVICE}/{profile.sh,profile.ps1} ${OUT}/unlock/amonet/
  chmod +x ${OUT}/unlock/amonet/fastbrick.sh

  set +e
  cp bin/fastboot* ${OUT}/unlock/amonet/bin/ 2>/dev/null || true
  cp bin/*.dll ${OUT}/unlock/amonet/bin/ 2>/dev/null || true
  cp bin/${DEVICE}/full*.img ${OUT}/unlock/amonet/bin/ 2>/dev/null || true
  set -e

  mkdir -p ${OUT}/full
  cp -r ${OUT}/unlock/* ${OUT}/full/

  for f in bin/preloader.img bin/lk.bin bin/tz.img bin/tee-payload.bin bin/${DEVICE}-kaeru.bin bin/twrp.img \
           device.prop fastbrick.sh fastbrick.bat fastbrick.ps1 profile.sh profile.ps1; do
    if [ ! -f ${OUT}/full/amonet/$f ]; then
      echo "error: ${OUT}/full/amonet/$f is missing, refusing to build $ZIP_NAME" >&2
      exit 1
    fi
  done

  if ! ls ${OUT}/full/amonet/bin/fastbrick*.img >/dev/null 2>&1; then
    echo "error: no fastbrick image in ${OUT}/full/amonet/bin, refusing to build $ZIP_NAME" >&2
    exit 1
  fi

  (cd ${OUT}/full && zip -qr ../${ZIP_NAME} amonet META-INF)
  mv ${OUT}/${ZIP_NAME} ${OUT}/full/${ZIP_NAME}

  echo "  -> ${OUT}/full/${ZIP_NAME}"
done
