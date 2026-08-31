import os
import sys
import time
import threading

from common import Device
from gpt import parse_gpt_compat
from logger import log

VERIFY_BLOCKS = 8

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

def switch_boot1(dev):
    dev.emmc_switch(2)

def progress(done, total, start_time):
    elapsed = time.time() - start_time
    speed = (done * 0x200) / elapsed / (1024 * 1024) if elapsed > 0 else 0
    print("[{} / {}] {:.1f}% at {:.2f} MB/s   ".format(done, total, done * 100 / total, speed), end='\r', flush=True)

def check_multi_read(dev, start_block):
    verify = min(VERIFY_BLOCKS, dev.max_blocks)
    multi = dev.emmc_read_blocks(start_block, verify)
    single = b"".join(dev.emmc_read(start_block + i) for i in range(verify))
    return multi == single

def check_multi_write(dev, start_block, chunk):
    verify = min(VERIFY_BLOCKS, len(chunk) // 0x200)
    written = b"".join(dev.emmc_read(start_block + i) for i in range(verify))
    return written == chunk[:verify * 0x200]

def flash_data(dev, data, start_block, max_size=0):
    while len(data) % 0x200 != 0:
        data += b"\x00"

    if max_size and len(data) > max_size:
        raise RuntimeError("data too big to flash")

    blocks = len(data) // 0x200
    multi = True
    x = 0
    start_time = time.time()

    while x < blocks:
        run = min(dev.max_blocks, blocks - x)
        chunk = data[x * 0x200:(x + run) * 0x200]

        if multi:
            dev.emmc_write_blocks(start_block + x, chunk)

            if x == 0 and not check_multi_write(dev, start_block, chunk):
                log("multi block write did not read back, falling back to single blocks")
                multi = False
                continue
        else:
            for i in range(run):
                dev.emmc_write(start_block + x + i, chunk[i * 0x200:(i + 1) * 0x200])

        x += run
        progress(x, blocks, start_time)
    print("")

def flash_binary(dev, path, start_block, max_size=0):
    with open(path, "rb") as fin:
        data = fin.read()
    while len(data) % 0x200 != 0:
        data += b"\x00"

    flash_data(dev, data, start_block, max_size=max_size)

def dump_binary(dev, path, start_block, max_size=0):
    blocks = max_size // 0x200
    multi = check_multi_read(dev, start_block)
    if not multi:
        log("multi block access is broken, falling back to single blocks")

    with open(path, "w+b") as fout:
        x = 0
        start_time = time.time()

        while x < blocks:
            run = min(dev.max_blocks, blocks - x)

            if multi:
                chunk = dev.emmc_read_blocks(start_block + x, run)
            else:
                chunk = b"".join(dev.emmc_read(start_block + x + i) for i in range(run))

            fout.write(chunk)
            x += run
            progress(x, blocks, start_time)
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

def parse_gpt(dev):
    data = dev.emmc_read_blocks(1, 33)

    parts, _, _ = parse_gpt_compat(data)

    return parts
