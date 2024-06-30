function check_device() {
  product=$(adb shell getprop ro.build.product | dos2unix)

  if [ "$product" != "${1}" ] ; then
    echo "This is only for the \"${1}\"${2}, your device is a \"${product}\""
    exit 1
  fi
}