#!/usr/bin/env python3

import sys
import time

from common import Device
from logger import log
from load_payload import load_payload, load_pl_payload
from functions import *

import usb.core
import usb.util

import ctypes

import traceback


import struct
import os

def prepare(dev):

    if dev.preloader:
        load_pl_payload(dev)
    else:
        load_payload(dev)

    device_type_id = dev.idme_read(b"device_type_id").rstrip(b"\x00").decode("utf-8")

    log("Check device_type_id")
    if device_type_id == "A31DTMEEVDDOIV":
        log("Detected sheldon (" + device_type_id + ")")
    elif device_type_id == "A265XOI9586NML":
        log("Detected sheldonp (" + device_type_id + ")")
    else:
        log("Wrong device detected: " + device_type_id)
        exit(1)

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
        flash_binary(dev, "../bin/gpt-sheldon.bin", 0, 34 * 0x200)

    # 1) Sanity check GPT
    log("Check GPT")
    switch_user(dev)

    # 1.1) Parse gpt
    gpt = parse_gpt(dev)
    log("gpt_parsed = {}".format(gpt))
    if "lk" not in gpt or "tee1" not in gpt or "boot" not in gpt or "recovery" not in gpt:
        raise RuntimeError("bad gpt")

    # 2) Sanity check boot0
    log("Check boot0")
    switch_boot0(dev)

    # 3) Sanity check rpmb
    log("Check rpmb")
    rpmb = dev.rpmb_read()
    if rpmb[0:4] != b"AMZN":
        thread = UserInputThread(msg = "rpmb looks broken; if this is expected (i.e. you're retrying the exploit) press enter, otherwise terminate with Ctrl+C")
        thread.start()
        while not thread.done:
            dev.kick_watchdog()
            time.sleep(1)

    # 5) Zero out rpmb to enable downgrade
    log("Downgrade rpmb")
    dev.rpmb_write(b"\x00" * 0x100)
    log("Recheck rpmb")
    rpmb = dev.rpmb_read()
    if rpmb != b"\x00" * 0x100:
        dev.reboot()
        raise RuntimeError("downgrade failure, giving up")
    log("rpmb downgrade ok")
    dev.kick_watchdog()

    # 6) Downgrade tz
    log("Flash tz")
    switch_user(dev)
    flash_binary(dev, "../bin/tz.img", gpt["tee1"][0], gpt["tee1"][1] * 0x200)

    # 7) Downgrade lk
    log("Flash lk")
    switch_user(dev)
    flash_binary(dev, "../bin/lk.bin", gpt["lk"][0], gpt["lk"][1] * 0x200)

    log("Force fastboot")
    force_fastboot(dev, gpt)

    if not dev.preloader:
        # 9) Install preloader
        log("Flash preloader")
        switch_boot0(dev)
        flash_binary(dev, "../bin/preloader.img", 0)

    # 9.1) Wait some time so data is flushed to EMMC
    time.sleep(5)

    # Reboot (to fastboot or recovery)
    log("Reboot")
    dev.reboot()


if __name__ == "__main__":

    check_modemmanager()

    args = [arg for arg in sys.argv[1:] if arg != "crash"]

    if args and args[0] in ("flash", "read"):
        if len(args) != 3:
            raise RuntimeError("{} needs a partition and a file".format(args[0]))
        if args[0] == "flash" and not os.path.exists(args[2]):
            raise RuntimeError("no such file: {}".format(args[2]))
    elif args and args != ["fixgpt"]:
        raise RuntimeError("unknown command: {}".format(args[0]))

    dev = Device()
    dev.find_device()

    if "crash" in sys.argv[1:]:
        while dev.preloader:
            log("Found device in preloader mode, trying to crash...")
            dev.handshake()
            dev.crash_pl()
            dev.dev.close()
            dev = Device()
            dev.find_device()

    if args and args[0] == "flash":
        flash_partition(dev, args[1], args[2])
    elif args and args[0] == "read":
        read_partition(dev, args[1], args[2])
    else:
        main(dev)
