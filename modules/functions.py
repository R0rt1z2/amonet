import struct
import os
import sys
import time
import threading

from common import BLOCKS_PER_WRITE, Device
from logger import log

class UserInputThread(threading.Thread):

    def __init__(self, msg = "* * * If you have a short attached, remove it now * * *\n* * * Press Enter to continue * * *", *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.done = False
        self.msg = msg

    def run(self):
        print("")
        print(self.msg)
        print("")
        input()
        self.done = True

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

def switch_boot1(dev):
    dev.emmc_switch(2)
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

def flash_binary(dev, path, start_block, max_size=0):
    with open(path, "rb") as fin:
        data = fin.read()
    while len(data) % 0x200 != 0:
        data += b"\x00"

    flash_data(dev, data, start_block, max_size=max_size)

def dump_binary(dev, path, start_block, max_size=0):
    with open(path, "w+b") as fout:
        blocks = max_size // 0x200
        for x in range(blocks):
            print("[{} / {}]".format(x + 1, blocks), end='\r', flush=True)
            fout.write(dev.emmc_read(start_block + x))
            if x % 10 == 0:
                dev.kick_watchdog()
    print("")

def find_misc(gpt):
    for name in gpt:
        if name.lower() == "misc":
            return gpt[name]
    raise RuntimeError("no misc partition (have: {})".format(", ".join(sorted(gpt))))

def write_misc(dev, gpt, data):
    start_block = find_misc(gpt)[0]
    switch_user(dev)
    block = list(dev.emmc_read(start_block))
    block[0:len(data)] = data
    dev.emmc_write(start_block, bytes(block))
    block = dev.emmc_read(start_block)

def force_fastboot(dev, gpt):
    write_misc(dev, gpt, "FASTBOOT_PLEASE\x00".encode("utf-8"))

def temp_fastboot(dev, gpt):
    write_misc(dev, gpt, "boot-amonet\x00\x00\x00\x00\x00".encode("utf-8"))

def force_recovery(dev, gpt):
    write_misc(dev, gpt, "boot-recovery\x00\x00\x00".encode("utf-8"))

def clear_flags(dev, gpt):
    write_misc(dev, gpt, b"\x00" * 32)

def switch_user(dev):
    dev.emmc_switch(0)
    block = dev.emmc_read(0)
    if block[510:512] != b"\x55\xAA":
        dev.reboot()
        raise RuntimeError("what's wrong with your GPT?")
    dev.kick_watchdog()

def parse_gpt(dev):
    data = dev.emmc_read(0x400 // 0x200) + dev.emmc_read(0x600 // 0x200) + dev.emmc_read(0x800 // 0x200) + dev.emmc_read(0xA00 // 0x200)
    num = len(data) // 0x80
    parts = dict()
    for x in range(num):
        part = data[x * 0x80:(x + 1) * 0x80]
        part_name = part[0x38:].decode("utf-16le").rstrip("\x00")
        part_start = struct.unpack("<Q", part[0x20:0x28])[0]
        part_end = struct.unpack("<Q", part[0x28:0x30])[0]
        parts[part_name] = (part_start, part_end - part_start + 1)
    return parts
