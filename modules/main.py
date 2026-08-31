#!/usr/bin/env python3

import os
import sys
import time

from common import Device
from logger import log
from load_payload import load_pl_payload
from functions import *

DEVICE_TYPE_IDS = {
    "A2QCPPMSOLGVZE": "Fire Max 11 (2023)",
}

def prepare(dev):

    if not dev.preloader:
        raise RuntimeError("device is not in preloader mode")

    load_pl_payload(dev)

    device_type_id = dev.idme_read(b"device_type_id").rstrip(b"\x00").decode("utf-8")

    if device_type_id not in DEVICE_TYPE_IDS:
        supported = ", ".join("{} ({})".format(name, did) for did, name in DEVICE_TYPE_IDS.items())
        raise RuntimeError("device_type_id is {}, but this exploit only supports: {}".format(device_type_id, supported))

    log("Detected {} ({})".format(DEVICE_TYPE_IDS[device_type_id], device_type_id))


def find_partition(dev, name):

    switch_user(dev)

    gpt = parse_gpt(dev)
    if name not in gpt:
        raise RuntimeError("no such partition: {} (have: {})".format(name, ", ".join(sorted(gpt))))

    return gpt[name]

def flash_partition(dev, name, path):

    prepare(dev)

    start_block, blocks = find_partition(dev, name)
    log("Flash {} to {} ({} blocks at block {})".format(path, name, blocks, start_block))
    flash_binary(dev, path, start_block, blocks * 0x200)

    time.sleep(5)

    log("Reboot")
    dev.reboot()

def read_partition(dev, name, path):

    prepare(dev)

    start_block, blocks = find_partition(dev, name)
    log("Dump {} ({} blocks at block {}) to {}".format(name, blocks, start_block, path))
    dump_binary(dev, path, start_block, blocks * 0x200)

    log("Reboot")
    dev.reboot()

def main(dev):

    prepare(dev)

    if "fixgpt" in sys.argv[1:]:
        dev.emmc_switch(0)
        log("Flashing GPT")
        flash_binary(dev, "../bin/gpt-karat.bin", 0, 34 * 0x200)

    log("Check GPT")
    switch_user(dev)

    gpt = parse_gpt(dev)
    for part in ("mcupm", "lk", "tee1", "tee2"):
        if part not in gpt:
            raise RuntimeError("bad gpt")
    find_misc(gpt)

    log("Flash mcupm")
    switch_user(dev)
    flash_binary(dev, "../bin/mcupm.img", gpt["mcupm"][0], gpt["mcupm"][1] * 0x200)

    log("Flash payload")
    switch_user(dev)
    flash_binary(dev, "../bin/karat-kaeru.bin", gpt["lk"][0], gpt["lk"][1] * 0x200)

    log("Flash tee1")
    switch_user(dev)
    flash_binary(dev, "../bin/tz.img", gpt["tee1"][0], gpt["tee1"][1] * 0x200)

    log("Flash tee2")
    switch_user(dev)
    flash_binary(dev, "../bin/tz.img", gpt["tee2"][0], gpt["tee2"][1] * 0x200)

    log("Force fastboot")
    force_fastboot(dev, gpt)

    time.sleep(5)

    log("Reboot to fastboot")
    dev.reboot()


if __name__ == "__main__":

    check_modemmanager()

    args = sys.argv[1:]

    if args and args[0] in ("flash", "read"):
        if len(args) != 3:
            raise RuntimeError("{} needs a partition and a file".format(args[0]))
        args[2] = os.path.abspath(args[2])
        if args[0] == "flash" and not os.path.exists(args[2]):
            raise RuntimeError("no such file: {}".format(args[2]))
    elif args and args != ["fixgpt"]:
        raise RuntimeError("unknown command: {}".format(args[0]))

    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    dev = Device()
    dev.find_device()

    if args and args[0] == "flash":
        flash_partition(dev, args[1], args[2])
    elif args and args[0] == "read":
        read_partition(dev, args[1], args[2])
    else:
        main(dev)
