function get_root() {
  set +e
  echo "Testing root access..."
  adb shell su -c id | grep uid=0\(root\)
  ROOT=$?
  if [ $ROOT -ne 0 ]; then
    echo ""
    echo "Trying to use mtk-su to get temp root..."
    check_su
    push_root
    while [ $ROOT -ne 0 ]; do
      adb shell /data/local/tmp/mtk-su -c "/data/local/tmp/busybox mount /data/local/tmp/minisu.img /system/xbin/"
      adb shell su -c id | grep uid=0\(root\)
      ROOT=$?
      [ $ROOT -ne 0 ] && sleep 1
    done
  fi
  echo ""
  set -e
}

function check_su() {
  if [ ! -f "bin/mtk-su" ]; then
    echo "No mtk-su found, you can get it here: https://forum.xda-developers.com/hd8-hd10/orig-development/experimental-software-root-hd-8-hd-10-t3904595"
    exit 1
  fi
}

function push_root() {
  echo "Pushing root files"
  adb push bin/mtk-su /data/local/tmp/
  adb shell chmod 755 /data/local/tmp/mtk-su
  adb push bin/minisu.img /data/local/tmp/
  adb push bin/busybox /data/local/tmp/
  adb shell chmod 755 /data/local/tmp/busybox
}

function check_device() {
  product=$(adb shell getprop ro.product.name | dos2unix)

  if [ "$product" != "${1}" ] ; then
    echo "This is only for the \"${1}\"${2}, your device is a \"${product}\""
    exit 1
  fi
}
