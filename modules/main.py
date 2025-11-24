#!/usr/bin/env python3
import os
import struct
import sys
import time
from argparse import ArgumentParser

from common import Device
from functions import *
from gpt import (
    generate_gpt,
    modify_step1,
    modify_step2,
    parse_gpt_compat,
)
from gpt import (
    parse_gpt as gpt_parse_gpt,
)
from handshake import handshake
from load_payload import load_payload
from logger import log


def main(dev, args):
    check_modemmanager()
    
    dev.find_device()
    
    if dev.preloader:
        log("Crashing preloader to bootrom...")
        dev.handshake()
        dev.crash_pl()
        dev.dev.close()
        dev.dev = None
        
        time.sleep(0.5)
        dev.find_device('0E8D', '0003')
        
        if dev.preloader:
            raise RuntimeError("Failed to crash to bootrom")
    
    handshake(dev, args.skip_handshake)

    # 0.2) Load brom payload
    load_payload(dev, "../brom-payload/build/payload.bin")
    dev.kick_watchdog()

    if args.gptfix:
        dev.emmc_switch(0)
        log('Flashing GPT')
        flash_binary(dev, '../bin/gpt-sloane.bin', 0, 34 * 0x200)

    # 1) Sanity check GPT
    log('Check GPT')
    switch_user(dev)

    # 1.1) Parse gpt
    gpt = parse_gpt(dev)
    log("gpt_parsed = {}".format(gpt))
    if "lk" not in gpt or "tee1" not in gpt or "boot" not in gpt or "recovery" not in gpt:
        raise RuntimeError("bad gpt")

    if args.image:
        log('Flash {}'.format(args.partition))
        flash_partition(
            dev,
            gpt,
            args.partition,
            os.path.abspath(args.image).replace('modules/', ''),
        )

        log('Reboot')
        return dev.reboot()

    # 2) Sanity check boot0
    log('Check boot0')
    switch_boot0(dev)

    # 3) Install lk-payload
    log("Flash lk-payload")
    switch_boot0(dev)
    flash_binary(dev, "../lk-payload/build/payload.bin", 0x200000 // 0x200)

    # 4) Inject microloader
    log("Inject microloader")
    switch_user(dev)
    boot_hdr1 = dev.emmc_read(gpt["boot"][0]) + dev.emmc_read(gpt["boot"][0] + 1)
    boot_hdr2 = dev.emmc_read(gpt["boot"][0] + 2) + dev.emmc_read(gpt["boot"][0] + 3)
    flash_binary(dev, "../bin/microloader.bin", gpt["boot"][0], 2 * 0x200)
    if boot_hdr2[0:8] != b"ANDROID!":
        flash_data(dev, boot_hdr1, gpt["boot"][0] + 2, 2 * 0x200)

    recovery_hdr1 = dev.emmc_read(gpt["recovery"][0]) + dev.emmc_read(gpt["recovery"][0] + 1)
    recovery_hdr2 = dev.emmc_read(gpt["recovery"][0] + 2) + dev.emmc_read(gpt["recovery"][0] + 3)
    flash_binary(dev, "../bin/microloader.bin", gpt["recovery"][0], 2 * 0x200)
    if recovery_hdr2[0:8] != b"ANDROID!":
        flash_data(dev, recovery_hdr1, gpt["recovery"][0] + 2, 2 * 0x200)

    # 5) Wait some time so data is flushed to EMMC
    time.sleep(5)

    # Reboot (to fastboot)
    log('Reboot to unlocked fastboot')
    dev.reboot()


if __name__ == '__main__':
    arg_parser = ArgumentParser()
    arg_parser.add_argument(
        '--port', type=str, default=None, help='Serial port'
    )
    arg_parser.add_argument(
        '--skip-handshake',
        '-s',
        action='store_true',
        default=False,
        help='Skip handshake',
    )
    arg_parser.add_argument(
        '--partition', '-p', type=str, help='Partition name to flash'
    )
    arg_parser.add_argument(
        '--image', '-f', type=str, help='Image file to flash to the partition'
    )
    arg_parser.add_argument(
        '--gptfix', '-g', action='store_true', help='Fix GPT'
    )
    args = arg_parser.parse_args()

    if args.image and not args.partition:
        arg_parser.error('--partition is required when --image is provided')

    dev = Device(args.port)
    main(dev, args)
