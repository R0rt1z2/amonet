import glob
import struct
import os
import sys
import time

from common import BLOCKS_PER_WRITE, Device
from handshake import handshake
from gpt import parse_gpt_compat
from load_payload import load_payload, load_pl_payload, UserInputThread
from logger import log

DEVICES = {
    "A4ZP7ZC4PI6TO":  ("checkers", "Amazon Echo Show 5 (2019 / 1st Gen)"),
    "A1Z88NGR2BK6A2": ("crown",    "Amazon Echo Show 8 (2019 / 1st Gen)"),
    "A1XWJRHALS1REP": ("cronos",   "Amazon Echo Show 5 (2021 / 2nd Gen)"),
}

def check_brom(dev):
    devinfo = struct.unpack("<I", dev.mem_read(0x10206060, 4))[0]
    disabled = (devinfo >> 8) & 1

    log("BROM USBDL is {}".format("disabled" if disabled else "enabled"))

    return not disabled

def warn_and_continue(dev, msg):
    thread = UserInputThread(msg = msg + "; if this is expected press enter, otherwise terminate with Ctrl+C")
    thread.start()
    while not thread.done:
        dev.kick_watchdog()
        time.sleep(1)

def device_name(device_type_id):
    if device_type_id not in DEVICES:
        return "an unknown device ({})".format(device_type_id)

    codename, model = DEVICES[device_type_id]

    return "{} - ({})".format(model, codename)

def read_device_prop(path = "../device.prop"):
    props = dict()

    try:
        with open(path) as fin:
            for line in fin:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                props[key.strip()] = value.strip().strip('"')
    except IOError:
        pass

    return props

def identify_device(dev):
    expected = read_device_prop().get("DEVICE_TYPE_ID")

    device_type_id = dev.idme_read(b"device_type_id")
    if device_type_id is None:
        warn_and_continue(dev, "there is no device_type_id in IDME, so this might not be a device we support")
        return None

    device_type_id = device_type_id.rstrip(b"\x00").decode("utf-8")

    if expected and device_type_id != expected:
        raise RuntimeError("this package is for {}, but this is {}, refusing to flash it".format(device_name(expected), device_name(device_type_id)))

    if device_type_id not in DEVICES:
        warn_and_continue(dev, "unknown device (device_type_id = {}), flashing it could brick it".format(device_type_id))
        return None

    codename, model = DEVICES[device_type_id]
    log("Detected {}".format(device_name(device_type_id)))

    return codename

def check_modemmanager():
    pids = [pid for pid in os.listdir('/proc') if pid.isdigit()]

    for pid in pids:
        try:
            args = open(os.path.join('/proc', pid, 'cmdline'), 'rb').read().decode("utf-8").split('\0')
            if len(args) > 0 and "modemmanager" in args[0].lower():
                print("You need to temporarily disable/uninstall ModemManager before this script can proceed")
                sys.exit(1)
        except IOError:
            continue

def switch_boot0(dev):
    dev.emmc_switch(1)
    block = dev.emmc_read(0)
    if block[0:9] != b"EMMC_BOOT" and block != b"\x00" * 0x200:
        dev.reboot()
        raise RuntimeError("what's wrong with your BOOT0?")
    dev.kick_watchdog()

def flash_data(dev, data, start_block, max_size=0):
    while len(data) % 0x200 != 0:
        data += b"\x00"

    if max_size and len(data) > max_size:
        raise RuntimeError("data too big to flash")

    blocks = len(data) // 0x200
    multi = True
    x = 0

    while x < blocks:
        run = min(BLOCKS_PER_WRITE, blocks - x)
        chunk = data[x * 0x200:(x + run) * 0x200]

        if multi:
            dev.emmc_write_blocks(start_block + x, chunk)

            if x == 0:
                written = b"".join(dev.emmc_read(start_block + i) for i in range(run))
                if written != chunk:
                    log("multi block write did not read back, falling back to single blocks")
                    multi = False
                    continue
        else:
            for i in range(run):
                dev.emmc_write(start_block + x + i, chunk[i * 0x200:(i + 1) * 0x200])

        x += run
        print("[{} / {}]".format(x, blocks), end='\r', flush=True)
        dev.kick_watchdog()
    print("")

def find_binary(name):
    device = os.environ.get("DEVICE")
    if device:
        roots = ["../bin/{}".format(device)]
    else:
        roots = ["../bin", "../bin/*"]

    matches = []
    for root in roots:
        matches += sorted(glob.glob("{}/{}".format(root, name)))

    if not matches:
        raise RuntimeError("no file matching {} found in bin/".format(name))
    if len(matches) > 1:
        raise RuntimeError("multiple files matching {}: {}; set DEVICE to pick one".format(name, ", ".join(matches)))
    return matches[0]

def flash_binary(dev, path, start_block, max_size=0):
    with open(path, "rb") as fin:
        data = fin.read()
    while len(data) % 0x200 != 0:
        data += b"\x00"

    flash_data(dev, data, start_block, max_size=0)

def dump_binary(dev, path, start_block, max_size=0):
    with open(path, "w+b") as fout:
        blocks = max_size // 0x200
        for x in range(blocks):
            print("[{} / {}]".format(x + 1, blocks), end='\r', flush=True)
            fout.write(dev.emmc_read(start_block + x))
        if x % 10 == 0:
            dev.kick_watchdog()
    print("")

def force_fastboot(dev, gpt):
    switch_user(dev)
    block = list(dev.emmc_read(gpt["MISC"][0]))
    block[0:16] = "FASTBOOT_PLEASE\x00".encode("utf-8")
    dev.emmc_write(gpt["MISC"][0], bytes(block))
    block = dev.emmc_read(gpt["MISC"][0])

def force_recovery(dev, gpt):
    switch_user(dev)
    block = list(dev.emmc_read(gpt["MISC"][0]))
    block[0:16] = "boot-recovery\x00\x00\x00".encode("utf-8")
    dev.emmc_write(gpt["MISC"][0], bytes(block))
    block = dev.emmc_read(gpt["MISC"][0])

def switch_user(dev):
    dev.emmc_switch(0)
    block = dev.emmc_read(0)
    if block[510:512] != b"\x55\xAA":
        dev.reboot()
        raise RuntimeError("what's wrong with your GPT?")
    dev.kick_watchdog()

def parse_gpt(dev):
    data = b""
    for block in range(1, 34):
        data += dev.emmc_read(block)
        if block % 10 == 0:
            dev.kick_watchdog()

    parts, _, _ = parse_gpt_compat(data)

    return parts

def main():
    check_modemmanager()

    dev = Device()
    dev.find_device()

    # 0.1) Handshake
    handshake(dev)

    # 0.2) Load the payload, the way we get it in depends on where we are
    if dev.preloader:
        load_pl_payload(dev, "../brom-payload/build/pl.bin")
    else:
        load_payload(dev, "../brom-payload/build/payload.bin")
    dev.kick_watchdog()

    # 0.3) Figure out what we are talking to
    identify_device(dev)
    check_brom(dev)
    dev.kick_watchdog()

    if len(sys.argv) == 2 and sys.argv[1] == "fixgpt":
        dev.emmc_switch(0)
        log("Flashing GPT")
        flash_binary(dev, find_binary("gpt-*.bin"), 0, 34 * 0x200)

    # 1) Sanity check GPT
    log("Check GPT")
    switch_user(dev)

    # 1.1) Parse gpt
    gpt = parse_gpt(dev)
    for part in ("lk", "tee1", "tee2", "expdb", "MISC"):
        if part not in gpt:
            raise RuntimeError("bad gpt, missing {}".format(part))

    # 2) Sanity check boot0
    log("Check boot0")
    switch_boot0(dev)

    # 2.1) Clear preloader so, we get into bootrom without shorting, should the script stall (we flash preloader as last step)
    if not dev.preloader:
        log("Clear preloader header")
        flash_data(dev, b"EMMC_BOOT" + b"\x00" * ((0x200 * 8) - 9), 0)

    # 3) Sanity check rpmb
    log("Check rpmb")
    rpmb = dev.rpmb_read()
    if rpmb[0:4] != b"AMZN":
        warn_and_continue(dev, "rpmb looks broken (i.e. you're retrying the exploit)")

    # 4) Zero out rpmb to enable downgrade
    log("Downgrade rpmb")
    dev.rpmb_write(b"\x00" * 0x100)
    log("Recheck rpmb")
    rpmb = dev.rpmb_read()
    if rpmb != b"\x00" * 0x100:
        dev.reboot()
        raise RuntimeError("downgrade failure, giving up")
    log("rpmb downgrade ok")
    dev.kick_watchdog()

    # 6) Flash original tee to tee2
    log("Flash tee2")
    switch_user(dev)
    flash_binary(dev, "../bin/tz.img", gpt["tee2"][0], gpt["tee2"][1] * 0x200)

    # 7) Flash original lk
    log("Flash lk")
    switch_user(dev)
    flash_binary(dev, "../bin/lk.bin", gpt["lk"][0], gpt["lk"][1] * 0x200)

    # 8) Flash kaeru
    log("Flash kaeru")
    switch_user(dev)
    flash_binary(dev, find_binary("*-kaeru.bin"), gpt["expdb"][0], gpt["expdb"][1] * 0x200)

    # 9) Flash tee w/ payload to tee1
    log("Flash payload")
    switch_user(dev)
    flash_binary(dev, "../bin/tee-payload.bin", gpt["tee1"][0], gpt["tee1"][1] * 0x200)

    # 10) Downgrade preloader
    if not dev.preloader:
        log("Flash preloader")
        switch_boot0(dev)
        flash_binary(dev, "../bin/preloader.img", 0)

    # 11) Force fastboot mode
    log("Force fastboot mode")
    force_fastboot(dev, gpt)

    # 12) Wait some time so data is flushed to EMMC
    for _ in range(5):
        dev.kick_watchdog()
        time.sleep(1)

    # Reboot (to fastboot)
    log("Reboot to unlocked fastboot")
    dev.reboot()


if __name__ == "__main__":
    main()