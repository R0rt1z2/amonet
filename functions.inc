function check_device() {
  product=$(adb shell getprop ro.build.product | dos2unix)

  if [ "$product" != "${1}" ] ; then
    echo "This is only for the \"${1}\"${2}, your device is a \"${product}\""
    exit 1
  fi
}

function check_root() {
    # FireOS does not have a decent id
    adb shell "[ -w /data/data ] && echo 1 || echo 0" | grep "1" > /dev/null
    
    if [ $? -eq 0 ]; then
        CMD_PREFIX=""
        CMD_SUFFIX=""
        echo "ADB already running as root"
    else
        CMD_PREFIX="su -c \""
        CMD_SUFFIX="\""
        echo "ADB not running as root, using su -c"
    fi
}