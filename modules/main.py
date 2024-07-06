#!/usr/bin/env python3
import sys
import struct
import os
import sys
import time

from argparse import ArgumentParser

from common import Device
from handshake import handshake
from logger import log
from functions import *
from gpt import (
    parse_gpt_compat,
    generate_gpt,
    modify_step1,
    modify_step2,
    parse_gpt as gpt_parse_gpt,
)
from mmc import Mmc


def main(dev, args):
    check_modemmanager()
    dev.find_device()

    # 0.1) Handshake
    handshake(dev, args.skip_handshake)

    # 0.2) Initialize eMMC
    log("Init emmc")
    dev.set_mmc(Mmc(dev, 0x11230000))

    if args.gptfix:
        switch_user(dev)
        log("Flashing GPT")
        flash_binary(
            dev,
            f"../bin/gpt-thempis-{args.size}G.bin",
            dev.mmc.gpt_start // dev.mmc.block_size,
        )

    # 1) Sanity check GPT
    log("Check GPT")
    switch_user(dev)

    # 1.1) Parse gpt
    gpt, gpt_header, part_list = parse_gpt(dev)
    # log("gpt_parsed = {}".format(gpt))
    if (
        "UBOOT" not in gpt
        or "TEE1" not in gpt
        or "TEE2" not in gpt
        or "boot" not in gpt
        or "recovery" not in gpt
    ):
        raise RuntimeError("bad gpt")

    if args.image:
        log("Flash {}".format(args.partition))
        flash_partition(
            dev,
            gpt,
            args.partition,
            os.path.abspath(args.image).replace("modules/", ""),
        )

        log("Reboot")
        return dev.reboot()

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
        flash_data(dev, primary, dev.mmc.gpt_start // dev.mmc.block_size)

        log("Flash new backup GPT")
        flash_data(
            dev,
            backup,
            (gpt_header["last_lba"] + 1) + (dev.mmc.gpt_start // dev.mmc.block_size),
        )

        gpt, gpt_header, part_list = parse_gpt(dev)
        # log("gpt_parsed = {}".format(gpt))
        if "boot_x" not in gpt or "recovery_x" not in gpt:
            raise RuntimeError("bad gpt")

    # 2) Sanity check boot0
    log("Check boot0")
    switch_boot0(dev)

    # 3) Flash TZ (includes preloader)
    log("Flash TZ")
    switch_user(dev)
    flash_partition(dev, gpt, "TEE1", "../bin/tz.img")

    # 4) Flash LK
    log("Flash lk")
    switch_user(dev)
    flash_partition(dev, gpt, "UBOOT", "../bin/lk.bin")

    # 5) Flash payload
    log("Inject payload")
    switch_user(dev)
    flash_binary(
        dev,
        "../bin/boot.hdr",
        gpt["boot"][0] + (dev.mmc.gpt_start // dev.mmc.block_size),
        gpt["boot"][1] * 0x200,
    )
    flash_binary(
        dev,
        "../bin/boot.payload",
        (gpt["boot"][0] + 57271) + (dev.mmc.gpt_start // dev.mmc.block_size),
        (gpt["boot"][1] * 0x200) - (57271 * 0x200),
    )

    switch_user(dev)
    flash_binary(
        dev,
        "../bin/boot.hdr",
        gpt["recovery"][0] + (dev.mmc.gpt_start // dev.mmc.block_size),
        gpt["recovery"][1] * 0x200,
    )
    flash_binary(
        dev,
        "../bin/boot.payload",
        (gpt["recovery"][0] + 57271) + (dev.mmc.gpt_start // dev.mmc.block_size),
        (gpt["recovery"][1] * 0x200) - (57271 * 0x200),
    )

    log("Force fastboot")
    force_fastboot(dev, gpt)

    # 6) Wait some time so data is flushed to EMMC
    time.sleep(5)

    # Reboot (to fastboot)
    log("Reboot to unlocked fastboot")
    dev.reboot()


if __name__ == "__main__":
    arg_parser = ArgumentParser()
    arg_parser.add_argument("--port", type=str, default=None, help="Serial port")
    arg_parser.add_argument(
        "--skip-handshake",
        "-s",
        action="store_true",
        default=False,
        help="Skip handshake",
    )
    arg_parser.add_argument(
        "--partition", "-p", type=str, help="Partition name to flash"
    )
    arg_parser.add_argument(
        "--image", "-f", type=str, help="Image file to flash to the partition"
    )
    arg_parser.add_argument("--gptfix", "-g", action="store_true", help="Fix GPT")
    arg_parser.add_argument(
        "--size", type=int, choices=[8, 16], help="Size of the storage device (8 or 16)"
    )
    args = arg_parser.parse_args()

    if args.image and not args.partition:
        arg_parser.error("--partition is required when --image is provided")

    if args.gptfix and args.size is None:
        arg_parser.error("--size is required when --gptfix is provided")

    dev = Device(args.port)
    main(dev, args)
