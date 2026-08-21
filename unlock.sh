#!/bin/bash

set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/bin/unlock"

die() {
    printf '\033[1;31m(!) %s\033[0m\n' "$*" >&2
    exit 1
}

clear 2>/dev/null || printf '\033[2J\033[3J\033[H'

printf '\033[1;35m%s\033[0m\n\n' "Amazon Fire TV Stick 4K 2nd Gen series unlock @ by R0rt1z2"

printf '\033[1;33m%s\033[0m\n' "This procedure carries a real chance of bricking the device."
printf '\033[1;33m%s\033[0m\n\n' "You accept that risk, the developer takes no responsibility."
read -r -p "$(printf '\033[1;37m%s\033[0m ' 'Do you want to continue? [y/N]')" ans
case "$ans" in
    y|Y|yes|YES) ;;
    *) exit 1 ;;
esac

[ -f "$BIN" ] || die "missing $BIN"

if command -v adb > /dev/null 2>&1; then
    ADB="adb"
else
    case "$(uname -s)" in
        Linux) ADB="$DIR/bin/adb-linux" ;;
        Darwin) ADB="$DIR/bin/adb-darwin" ;;
        *) die "Unsupported platform: $(uname -s)" ;;
    esac
    [ -f "$ADB" ] || die "missing $ADB"
    chmod 755 "$ADB"
fi

printf '\n%s\n\n' "Waiting for device..."

STATE=""
while [ -z "$STATE" ]; do
    STATE="$("$ADB" get-state 2>/dev/null || true)"
    if [ -z "$STATE" ]; then sleep 1; fi
done

if [ "$STATE" = "recovery" ]; then
    printf '\033[1;32m%s\033[0m\n' "Device is in TWRP, it is already unlocked."
    exit 0
fi

if ! "$ADB" shell 'su -c id' 2>/dev/null | grep -q 'uid=0'; then
    die "Please root your device with GhostLock first"
fi

clear 2>/dev/null || printf '\033[2J\033[3J\033[H'

"$ADB" push "$BIN" /data/local/tmp/unlock > /dev/null 2>&1 || die "Failed to push unlock"
"$ADB" shell 'chmod 755 /data/local/tmp/unlock' > /dev/null 2>&1
"$ADB" shell 'su -c /data/local/tmp/unlock' || true

printf '\033[1;32m%s\033[0m\n' "Done, device should reboot to TWRP."
