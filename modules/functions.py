import struct
import os
import sys
import time
import threading

from common import Device
from logger import log
from gpt import parse_gpt_compat

def check_modemmanager():
    pids = [pid for pid in os.listdir("/proc") if pid.isdigit()]

    for pid in pids:
        try:
            args = (
                open(os.path.join("/proc", pid, "cmdline"), "rb")
                .read()
                .decode("utf-8")
                .split("\0")
            )
            if len(args) > 0 and "modemmanager" in args[0].lower():
                print(
                    "You need to temporarily disable/uninstall ModemManager before this script can proceed"
                )
                sys.exit(1)
        except IOError:
            continue


def switch_boot0(dev):
    dev.mmc.set_part(1)
    block = dev.mmc.read_block(0)
    if block[0:9] != b"EMMC_BOOT" and block != b"\x00" * 0x200:
        dev.reboot()
        raise RuntimeError("what's wrong with your BOOT0?")

def switch_boot1(dev):
    dev.emmc_switch(2)
    dev.kick_watchdog()


def flash_data(dev, data, start_block, max_size=0):
    while len(data) % 0x200 != 0:
        data += b"\x00"

    if max_size and len(data) > max_size:
        raise RuntimeError("data too big to flash")

    blocks = len(data) // 0x200
    for x in range(blocks):
        print("[{} / {}]".format(x + 1, blocks), end="\r")
        dev.emmc_write(start_block + x, data[x * 0x200 : (x + 1) * 0x200])
    print("")


def flash_binary(dev, path, start_block, max_size=0):
    with open(path, "rb") as fin:
        data = fin.read()
    while len(data) % 0x200 != 0:
        data += b"\x00"

    flash_data(dev, data, start_block, max_size=0)


def dump_binary(dev, path, start, size):
    with open(path, "wb") as fout:
        for x in range(size // 0x200):
            print("[{} / {}]".format(x + 1, size // 0x200), end="\r")
            fout.write(dev.mmc.read_block(start + x))
    print("")


def force_fastboot(dev, gpt):
    switch_user(dev)
    block = list(dev.emmc_read(gpt["MISC"][0]))
    block[0:16] = "FASTBOOT_PLEASE\x00".encode("utf-8")
    dev.emmc_write(gpt["MISC"][0], bytes(block))
    block = dev.emmc_read(gpt["MISC"][0])


def temp_fastboot(dev, gpt):
    switch_user(dev)
    block = list(dev.emmc_read(gpt["MISC"][0]))
    block[0:16] = "boot-amonet\x00\x00\x00\x00\x00".encode("utf-8")
    dev.emmc_write(gpt["MISC"][0], bytes(block))
    block = dev.emmc_read(gpt["MISC"][0])


def force_recovery(dev, gpt):
    switch_user(dev)
    block = list(dev.emmc_read(gpt["MISC"][0]))
    block[0:16] = "boot-recovery\x00\x00\x00".encode("utf-8")
    dev.emmc_write(gpt["MISC"][0], bytes(block))
    block = dev.emmc_read(gpt["MISC"][0])


def clear_flags(dev, gpt):
    switch_user(dev)
    block = list(dev.emmc_read(gpt["MISC"][0]))
    block[0:32] = b"\x00" * 32
    dev.emmc_write(gpt["MISC"][0], bytes(block))
    block = dev.emmc_read(gpt["MISC"][0])


def switch_user(dev):
    dev.mmc.set_part(0)
    for i in [0x0A00000, 0x0400000, 0x0780000]:
        block = dev.mmc.read_block(i // 0x200)
        if block[510:512] == b"\x55\xAA":
            return dev.mmc.set_gpt_start(i + 0x200)

    raise RuntimeError("what's wrong with your GPT?")


def parse_gpt(dev):
    start = dev.mmc.gpt_start
    data = (
        dev.mmc.read_block((start + 0x200) // 0x200)
        + dev.mmc.read_block((start + 0x400) // 0x200)
        + dev.mmc.read_block((start + 0x600) // 0x200)
        + dev.mmc.read_block((start + 0x800) // 0x200)
        + dev.mmc.read_block((start + 0xA00) // 0x200)
        + dev.mmc.read_block((start + 0xC00) // 0x200)
    )
    return parse_gpt_compat(dev.mmc.read_block(start // 0x200) + data)
