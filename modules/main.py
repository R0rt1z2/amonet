#!/usr/bin/env python3
import sys
import struct
import os
import sys
import time

from common import Device
from handshake import handshake
from load_payload import load_payload
from logger import log
from functions import *
from gpt import parse_gpt_compat, generate_gpt, modify_step1, modify_step2, parse_gpt as gpt_parse_gpt
from mmc import Mmc

def main():
    # check_modemmanager()
    dev = Device()
    dev.find_device()

    # 0.1) Handshake
    handshake(dev)

    # 0.2) Initialize eMMC
    log("Init emmc")
    dev.set_mmc(Mmc(dev, 0x11230000))

    if len(sys.argv) == 2 and sys.argv[1] == "fixgpt":
        dev.emmc_switch(0)
        log("Flashing GPT")
        flash_binary(dev, "../bin/gpt-sloane.bin", 0, 34 * 0x200)

    # 1) Sanity check GPT
    log("Check GPT")
    switch_user(dev)

    # 1.1) Parse gpt
    gpt, gpt_header, part_list = parse_gpt(dev)
    #log("gpt_parsed = {}".format(gpt))
    if "UBOOT" not in gpt or "TEE1" not in gpt or "boot" not in gpt or "recovery" not in gpt:
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

    # 3) Flash TZ
    log("Flash tz")
    switch_user(dev)
    flash_binary(dev, "../bin/tz.img", gpt["TEE1"][0], gpt["TEE1"][1] * 0x200)

    # 4) Flash LK
    log("Flash lk")
    switch_user(dev)
    flash_binary(dev, "../bin/lk.bin", gpt["lk"][0], gpt["lk"][1] * 0x200)

    # 5) Flash payload
    log("Inject payload")
    switch_user(dev)
    flash_binary(dev, "../bin/boot.hdr", gpt["boot"][0], gpt["boot"][1] * 0x200)
    flash_binary(dev, "../bin/boot.payload", gpt["boot"][0] + 60407, (gpt["boot"][1] * 0x200) - (60407 * 0x200))
    
    switch_user(dev)
    flash_binary(dev, "../bin/boot.hdr", gpt["recovery"][0], gpt["recovery"][1] * 0x200)
    flash_binary(dev, "../bin/boot.payload", gpt["recovery"][0] + 60407, (gpt["recovery"][1] * 0x200) - (60407 * 0x200))

    log("Force fastboot")
    force_fastboot(dev, gpt)

    # 6) Wait some time so data is flushed to EMMC
    time.sleep(5)

    # Reboot (to fastboot)
    log("Reboot to unlocked fastboot")
    dev.reboot()

if __name__ == "__main__":
    main()