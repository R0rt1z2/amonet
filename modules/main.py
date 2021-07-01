#!/usr/bin/env python3
import sys
import struct
import os
import sys
import time

from common import Device
from handshake import handshake
from load_payload import load_payload, UserInputThread
from logger import log
from gpt import parse_gpt_compat, generate_gpt, modify_step1, modify_step2, parse_gpt as gpt_parse_gpt

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
    for x in range(blocks):
        print("[{} / {}]".format(x + 1, blocks), end='\r')
        dev.emmc_write(start_block + x, data[x * 0x200:(x + 1) * 0x200])
        if x % 10 == 0:
            dev.kick_watchdog()
    print("")

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
            print("[{} / {}]".format(x + 1, blocks), end='\r')
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

def switch_user(dev):
    dev.emmc_switch(0)
    block = dev.emmc_read(0)
    if block[510:512] != b"\x55\xAA":
        dev.reboot()
        raise RuntimeError("what's wrong with your GPT?")
    dev.kick_watchdog()

def parse_gpt(dev):
    data = b''
    for x in range(2, 34):
        data += dev.emmc_read(x)
    num = len(data) // 0x80
    return parse_gpt_compat(dev.emmc_read(0x200 // 0x200) + data)
#    parts = dict()
#    for x in range(num):
#        part = data[x * 0x80:(x + 1) * 0x80]
#        part_name = part[0x38:].decode("utf-16le").rstrip("\x00")
#        part_start = struct.unpack("<Q", part[0x20:0x28])[0]
#        part_end = struct.unpack("<Q", part[0x28:0x30])[0]
#        parts[part_name] = (part_start, part_end - part_start + 1)
#    return parts

def main():
    minimal = False

    check_modemmanager()

    dev = Device()
    dev.find_device()

    # 0.1) Handshake
    handshake(dev)

    # 0.2) Load brom payload
    load_payload(dev, "../brom-payload/build/payload.bin")
    dev.kick_watchdog()

    if len(sys.argv) == 2 and sys.argv[1] == "minimal":
        thread = UserInputThread(msg = "Running in minimal mode, assuming LK and TZ to have already been flashed.\nIf this is correct (i.e. you used \"brick\" option in step 1) press enter, otherwise terminate with Ctrl+C")
        thread.start()
        while not thread.done:
            dev.kick_watchdog()
            time.sleep(1)
        minimal = True

    if len(sys.argv) == 2 and sys.argv[1] == "fixgpt":
        dev.emmc_switch(0)
        log("Flashing GPT")
        flash_binary(dev, "../bin/gpt-douglas.bin", 0, 34 * 0x200)

    # 1) Sanity check GPT
    log("Check GPT")
    switch_user(dev)

    # 1.1) Parse gpt
    gpt, gpt_header, part_list = parse_gpt(dev)
    #log("gpt_parsed = {}".format(gpt))
    if "lk" not in gpt or "tee1" not in gpt or "boot" not in gpt or "recovery" not in gpt:
        raise RuntimeError("bad gpt")

    if "boot_x" not in gpt or "recovery_x" not in gpt:
        log("Modify GPT")

        if "boot_tmp" not in gpt and "recovery_tmp" not in gpt:
            part_list_mod1 = modify_step1(part_list)
        else:
            part_list_mod1 = part_list

        part_list_mod2 = modify_step2(part_list_mod1)
        primary, backup = generate_gpt(gpt_header, part_list_mod2)

        log("Validate GPT")
        gpt_header, part_list = gpt_parse_gpt(bytes(primary))

        log("Flash new primary GPT")
        flash_data(dev, primary, 0)

        log("Flash new backup GPT")
        flash_data(dev, backup, gpt_header['last_lba'] + 1)

        gpt, gpt_header, part_list = parse_gpt(dev)
        #log("gpt_parsed = {}".format(gpt))
        if "boot_x" not in gpt or "recovery_x" not in gpt:
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

    # Clear preloader so, we get into bootrom without shorting, should the script stall (we flash preloader as last step)
    # 10) Downgrade preloader
    log("Clear preloader header")
    switch_boot0(dev)
    flash_data(dev, b"EMMC_BOOT" + b"\x00" * ((0x200 * 8) - 9), 0)

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

    if not minimal:
        # 7) Downgrade tz
        log("Flash tz")
        switch_user(dev)
        flash_binary(dev, "../bin/tz.img", gpt["tee1"][0], gpt["tee1"][1] * 0x200)

        # 8) Downgrade lk
        log("Flash lk")
        switch_user(dev)
        flash_binary(dev, "../bin/lk.bin", gpt["lk"][0], gpt["lk"][1] * 0x200)

    # 9) Flash payload
    log("Inject payload")
    switch_user(dev)
    flash_binary(dev, "../bin/boot.hdr", gpt["boot"][0], gpt["boot"][1] * 0x200)
    flash_binary(dev, "../bin/boot.payload", gpt["boot"][0] + 223223, (gpt["boot"][1] * 0x200) - (223223 * 0x200))
    
    switch_user(dev)
    flash_binary(dev, "../bin/boot.hdr", gpt["recovery"][0], gpt["recovery"][1] * 0x200)
    flash_binary(dev, "../bin/boot.payload", gpt["recovery"][0] + 223223, (gpt["recovery"][1] * 0x200) - (223223 * 0x200))

    log("Force fastboot")
    force_fastboot(dev, gpt)

    # Flash preloader as last step, so we still have access to bootrom, should the script stall
    # 10) Downgrade preloader
    log("Flash preloader")
    switch_boot0(dev)
    flash_binary(dev, "../bin/boot0short.img", 0)
    flash_binary(dev, "../bin/preloader.bin", 520)

    # Reboot (to fastboot)
    log("Reboot to unlocked fastboot")
    dev.reboot()


if __name__ == "__main__":
    main()
