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

    gpt = parse_gpt(dev)
    log('== GPT start ==')
    for partition_name, partition_parameters in gpt.items():
        log('{} {}'.format(partition_name, partition_parameters))
    log('== GPT end ==')
    if (
        'lk' not in gpt
        or 'tee1' not in gpt
        or 'boot' not in gpt
        or 'recovery' not in gpt
    ):
        raise RuntimeError('bad gpt')

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

    """
    if 'boot_x' not in gpt or 'recovery_x' not in gpt:
        log('Modify GPT')

        if 'boot_tmp' not in gpt and 'recovery_tmp' not in gpt:
            part_list_mod1 = modify_step1(part_list)
        else:
            part_list_mod1 = part_list

        part_list_mod2 = modify_step2(part_list_mod1)
        primary, backup = generate_gpt(gpt_header, part_list_mod2)

        log('Validate GPT')
        gpt_header, part_list = gpt_parse_gpt(bytes(primary))

        log('Flash new primary GPT')
        flash_data(dev, primary, dev.mmc.gpt_start // dev.mmc.block_size)

        log('Flash new backup GPT')
        flash_data(
            dev,
            backup,
            (gpt_header['last_lba'] + 1)
            + (dev.mmc.gpt_start // dev.mmc.block_size),
        )

        gpt, gpt_header, part_list = parse_gpt(dev)
        # log("gpt_parsed = {}".format(gpt))
        if 'boot_x' not in gpt or 'recovery_x' not in gpt:
            raise RuntimeError('bad gpt')
    """

    # 2) Sanity check boot0
    log('Check boot0')
    switch_boot0(dev)

    # 6) Wait some time so data is flushed to EMMC
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
