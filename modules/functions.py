import os
import struct
import sys
import threading
import time

from common import Device
from gpt import parse_gpt_compat
from logger import log


def check_modemmanager():
    pids = [pid for pid in os.listdir('/proc') if pid.isdigit()]

    for pid in pids:
        try:
            args = (
                open(os.path.join('/proc', pid, 'cmdline'), 'rb')
                .read()
                .decode('utf-8')
                .split('\0')
            )
            if len(args) > 0 and 'modemmanager' in args[0].lower():
                print(
                    'You need to temporarily disable/uninstall ModemManager before this script can proceed'
                )
                sys.exit(1)
        except IOError:
            continue


def switch_boot0(dev):
    dev.mmc.set_part(1)
    block = dev.mmc.read_block(0)
    if block[0:9] != b'EMMC_BOOT' and block != b'\x00' * 0x200:
        dev.reboot()
        raise RuntimeError("what's wrong with your BOOT0?")


def switch_boot1(dev):
    dev.mmc.set_part(2)


def flash_data(dev, data, start_block, max_size=0):
    if max_size and len(data) > max_size:
        raise RuntimeError('data too big to flash')

    if len(data) % 512 != 0:
        data += b'\x00' * (512 - (len(data) % 512))

    blocks = len(data) // 512
    for x in range(blocks):
        print('[{} / {}]'.format(x + 1, blocks), end='\r')
        dev.mmc.write_block(start_block + x, data[x * 512 : (x + 1) * 512])
    print('')


def flash_binary(dev, path, start_block, max_size=0):
    with open(path, 'rb') as fin:
        data = fin.read()
    while len(data) % 0x200 != 0:
        data += b'\x00'

    flash_data(dev, data, start_block, max_size=max_size)


def dump_binary(dev, path, start, size):
    with open(path, 'wb') as fout:
        for x in range(size // 0x200):
            print('[{} / {}]'.format(x + 1, size // 0x200), end='\r')
            fout.write(dev.mmc.read_block(start + x))
    print('')


def force_fastboot(dev, gpt):
    switch_user(dev)
    block = list(
        dev.mmc.read_block(
            gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size)
        )
    )
    block[0:16] = 'FASTBOOT_PLEASE\x00'.encode('utf-8')
    dev.mmc.write_block(
        gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size), bytes(block)
    )


def temp_fastboot(dev, gpt):
    switch_user(dev)
    block = list(
        dev.mmc.read_block(
            gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size)
        )
    )
    block[0:16] = 'boot-amonet\x00\x00\x00\x00\x00'.encode('utf-8')
    dev.mmc.write_block(
        gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size), bytes(block)
    )
    block = dev.mmc.read_block(
        gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size)
    )
    print(block)


def force_recovery(dev, gpt):
    switch_user(dev)
    block = list(
        dev.mmc.read_block(
            gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size)
        )
    )
    block[0:16] = 'boot-recovery\x00\x00\x00'.encode('utf-8')
    dev.mmc.write_block(
        gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size), bytes(block)
    )
    block = dev.mmc.read_block(
        gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size)
    )
    print(block)


def clear_flags(dev, gpt):
    switch_user(dev)
    block = list(
        dev.mmc.read_block(
            gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size)
        )
    )
    block[0:32] = b'\x00' * 32
    dev.mmc.write_block(
        gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size), bytes(block)
    )
    block = dev.mmc.read_block(
        gpt['MISC'][0] + (dev.mmc.gpt_start // dev.mmc.block_size)
    )
    print(block)


def dump_partition(dev, gpt, name, output):
    def get_partition_info(name):
        special_parts = {
            'boot0': 0x2000,
            'preloader': 0x2000,
            'boot1': 0x2000,
            'idme': 0x2000,
        }
        return (
            (0, special_parts[name])
            if name.lower() in special_parts
            else gpt.get(name)
            or (
                lambda: (_ for _ in ()).throw(
                    RuntimeError('partition not found: {}'.format(name))
                )
            )()
        )

    start_sector, sector_count = get_partition_info(name)
    if name not in ['boot0', 'preloader', 'boot1', 'idme']:
        start_address = start_sector + (dev.mmc.gpt_start // dev.mmc.block_size)
    else:
        start_address = start_sector

    with open(output, 'wb') as fout:
        for sector_index, sector in enumerate(
            range(start_address, start_address + sector_count)
        ):
            block_data = dev.mmc.read_block(sector)
            fout.write(block_data)
            print('[{} / {}]'.format(sector_index + 1, sector_count), end='\r')

    print('')


def flash_partition(dev, gpt, name, input):
    def get_partition_info(name):
        special_parts = {
            'boot0': 0x2000,
            'preloader': 0x2000,
            'boot1': 0x2000,
            'idme': 0x2000,
        }
        return (
            (0, special_parts[name])
            if name.lower() in special_parts
            else gpt.get(name)
            or (
                lambda: (_ for _ in ()).throw(
                    RuntimeError('partition not found: {}'.format(name))
                )
            )()
        )

    start_sector, sector_count = get_partition_info(name)
    start_address = (
        start_sector + (dev.mmc.gpt_start // dev.mmc.block_size)
        if name.lower() not in ['boot0', 'preloader', 'boot1', 'idme']
        else start_sector
    )

    with open(input, 'rb') as fin:
        data = fin.read()

    if len(data) % 512 != 0:
        data += b'\x00' * (512 - (len(data) % 512))

    data_blocks = len(data) // 512

    if data_blocks > sector_count:
        raise RuntimeError('data too big to flash')

    for sector_index in range(data_blocks):
        sector = start_address + sector_index
        dev.mmc.write_block(
            sector, data[sector_index * 512 : (sector_index + 1) * 512]
        )
        print('[{} / {}]'.format(sector_index + 1, data_blocks), end='\r')

    print('')


def switch_user(dev):
    dev.mmc.set_part(0)
    for i in [0x0A00000, 0x0400000, 0x0780000]:
        block = dev.mmc.read_block(i // 0x200)
        if block[510:512] == b'\x55\xaa':
            return dev.mmc.set_gpt_start(i)

    raise RuntimeError("what's wrong with your GPT?")


def parse_gpt(dev):
    start = dev.mmc.gpt_start + 0x200
    data = (
        dev.mmc.read_block((start + 0x200) // 0x200)
        + dev.mmc.read_block((start + 0x400) // 0x200)
        + dev.mmc.read_block((start + 0x600) // 0x200)
        + dev.mmc.read_block((start + 0x800) // 0x200)
        + dev.mmc.read_block((start + 0xA00) // 0x200)
        + dev.mmc.read_block((start + 0xC00) // 0x200)
    )
    return parse_gpt_compat(dev.mmc.read_block(start // 0x200) + data)


def load_payload_file(path, align=4):
    with open(path, 'rb') as fin:
        payload = fin.read()
    log('Load payload from {} = 0x{:X} bytes'.format(path, len(payload)))
    while len(payload) % align != 0:
        payload += b'\x00'

    return payload


def hexdump(data, start_addr=0):
    for i in range(0, len(data), 16):
        address = start_addr + i
        line = f'{address:08x}  '

        hex_bytes = data[i : i + 16]
        hex_part = ' '.join(f'{b:02x}' for b in hex_bytes)
        hex_part = hex_part.ljust(48)

        ascii_part = ''.join(
            chr(b) if 32 <= b <= 126 else '.' for b in hex_bytes
        )
        print(f'{line}{hex_part} |{ascii_part}|')
