import os
import struct
import sys
import time

from common import BLOCKS_PER_READ, BLOCKS_PER_WRITE, Device
from handshake import handshake
from load_payload import load_payload, load_pl_payload, UserInputThread
from logger import log
from gpt import parse_gpt_compat, generate_gpt, unpatch, parse_gpt as gpt_parse_gpt

def check_brom(dev):
    devinfo = struct.unpack("<I", dev.mem_read(0x10206060, 4))[0]
    disabled = (devinfo >> 8) & 1

    log("BROM USBDL is {}".format("disabled" if disabled else "enabled"))

    return not disabled

def warn_and_continue(dev, msg):
    thread = UserInputThread(msg = msg + "; if this is expected press enter, otherwise terminate with Ctrl+C")
    thread.start()
    while not thread.done:
        dev.kick_watchdog()
        time.sleep(1)

VERIFY_BLOCKS = 8

def check_multi_read(dev, start_block):
    verify = min(VERIFY_BLOCKS, BLOCKS_PER_READ)
    multi = dev.emmc_read_blocks(start_block, verify)
    single = b"".join(dev.emmc_read(start_block + i) for i in range(verify))
    return multi == single

def check_multi_write(dev, start_block, chunk):
    verify = min(VERIFY_BLOCKS, len(chunk) // 0x200)
    written = b"".join(dev.emmc_read(start_block + i) for i in range(verify))
    return written == chunk[:verify * 0x200]

def progress(done, total, start_time):
    elapsed = time.time() - start_time
    speed = (done * 0x200) / elapsed / (1024 * 1024) if elapsed > 0 else 0
    print("[{} / {}] {:.1f}% at {:.2f} MB/s   ".format(done, total, done * 100 / total, speed), end='\r', flush=True)

def switch_boot0(dev):
    dev.emmc_switch(1)
    block = dev.emmc_read(0)
    if block[0:9] != b"EMMC_BOOT" and block[0:9] != b'\x00' * 9:
        dev.reboot()
        raise RuntimeError("what's wrong with your BOOT0?")
    dev.kick_watchdog()

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
        run = min(BLOCKS_PER_WRITE, blocks - x)
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

    flash_data(dev, data, start_block, max_size=0)

def dump_binary(dev, path, start_block, max_size=0):
    blocks = max_size // 0x200
    multi = check_multi_read(dev, start_block)
    if not multi:
        log("multi block read did not match, falling back to single blocks")

    with open(path, "w+b") as fout:
        x = 0
        start_time = time.time()

        while x < blocks:
            run = min(BLOCKS_PER_READ, blocks - x)

            if multi:
                chunk = dev.emmc_read_blocks(start_block + x, run)
            else:
                chunk = b"".join(dev.emmc_read(start_block + x + i) for i in range(run))

            fout.write(chunk)
            x += run
            progress(x, blocks, start_time)
    print("")

def force_fastboot(dev, gpt):
    switch_user(dev)
    block = list(dev.emmc_read(gpt["misc"][0]))
    block[0:16] = "FASTBOOT_PLEASE\x00".encode("utf-8")
    dev.emmc_write(gpt["misc"][0], bytes(block))
    block = dev.emmc_read(gpt["misc"][0])

def switch_user(dev):
    dev.emmc_switch(0)
    block = dev.emmc_read(0)
    if block[510:512] != b"\x55\xAA":
        dev.reboot()
        raise RuntimeError("what's wrong with your GPT?")
    dev.kick_watchdog()

def parse_gpt(dev):
    data = dev.emmc_read(0x400 // 0x200) + dev.emmc_read(0x600 // 0x200) + dev.emmc_read(0x800 // 0x200) + dev.emmc_read(0xA00 // 0x200) + dev.emmc_read(0xC00 // 0x200)
    num = len(data) // 0x80
    header = dev.emmc_read(0x200 // 0x200)
    dev.kick_watchdog()
    return parse_gpt_compat(header + data)
#    parts = dict()
#    for x in range(num):
#        part = data[x * 0x80:(x + 1) * 0x80]
#        part_name = part[0x38:].decode("utf-16le").rstrip("\x00")
#        part_start = struct.unpack("<Q", part[0x20:0x28])[0]
#        part_end = struct.unpack("<Q", part[0x28:0x30])[0]
#        parts[part_name] = (part_start, part_end - part_start + 1)
#    return parts

def prepare(dev):
    # 0.1) Handshake
    handshake(dev)

    # 0.2) Load the payload, the way we get it in depends on where we are
    if dev.preloader:
        load_pl_payload(dev, "../brom-payload/build/pl.bin")
    else:
        load_payload(dev, "../brom-payload/build/payload.bin")
    dev.kick_watchdog()

    # 0.3) Check the fast send path
    if dev.try_fast_send():
        log("Fast send path verified")
    else:
        log("Fast send path did not verify, staying on the stock routine")
    dev.kick_watchdog()

    # 0.4) Check whether the bootrom can still be entered
    check_brom(dev)
    dev.kick_watchdog()

def find_partition(dev, name):
    switch_user(dev)

    gpt, _, _ = parse_gpt(dev)
    if name not in gpt:
        raise RuntimeError("no such partition: {} (have: {})".format(name, ", ".join(sorted(gpt))))

    return gpt[name]

def flash_partition(dev, name, path):
    prepare(dev)

    start_block, blocks = find_partition(dev, name)

    size = os.path.getsize(path)
    if size > blocks * 0x200:
        raise RuntimeError("{} is 0x{:X} bytes, {} only holds 0x{:X}".format(path, size, name, blocks * 0x200))

    log("Flash {} to {} ({} blocks at block {})".format(path, name, blocks, start_block))
    flash_binary(dev, path, start_block, blocks * 0x200)

    # Wait some time so data is flushed to eMMC
    for _ in range(5):
        dev.kick_watchdog()
        time.sleep(1)

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

    if len(sys.argv) == 2 and sys.argv[1] == "fixgpt":
        dev.emmc_switch(0)
        log("Flashing GPT")
        flash_binary(dev, "../bin/gpt-biscuit.bin", 0, 34 * 0x200)
        # reboot
        dev.reboot()

    # 1) Sanity check GPT
    log("Check GPT")
    switch_user(dev)

    # 1.1) Parse gpt
    gpt, gpt_header, part_list = parse_gpt(dev)
    for part in ("lk_a", "lk_b", "tee1", "tee2", "expdb", "misc", "recovery"):
        if part not in gpt:
            raise RuntimeError("bad gpt, missing {}".format(part))

    # 1.2) Undo the partition table an older amonet patched in
    if "boot_a_x" in gpt or "boot_b_x" in gpt:
        log("Restore GPT")

        part_list_restored = unpatch(gpt_header, part_list)
        primary, backup = generate_gpt(gpt_header, part_list_restored)

        log("Validate GPT")
        gpt_header, part_list = gpt_parse_gpt(bytes(primary))

        log("Flash new primary GPT")
        flash_data(dev, primary, 0)

        log("Flash new backup GPT")
        flash_data(dev, backup, gpt_header['last_lba'] + 1)

        gpt, gpt_header, part_list = parse_gpt(dev)
        if "boot_a_x" in gpt or "boot_b_x" in gpt:
            raise RuntimeError("bad gpt")

    # 2) Sanity check boot0
    log("Check boot0")
    switch_boot0(dev)

    # 3) Sanity check rpmb
    log("Check rpmb")
    rpmb = dev.rpmb_read()
    if rpmb[0:4] != b"AMZN":
        warn_and_continue(dev, "rpmb looks broken (i.e. you're retrying the exploit)")

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

    # 5) Flash original tee to tee2
    log("Flash tee2")
    switch_user(dev)
    flash_binary(dev, "../bin/tz.img", gpt["tee2"][0], gpt["tee2"][1] * 0x200)

    # 6) Flash original lk to both slots
    log("Flash lk")
    switch_user(dev)
    flash_binary(dev, "../bin/lk.bin", gpt["lk_a"][0], gpt["lk_a"][1] * 0x200)
    flash_binary(dev, "../bin/lk.bin", gpt["lk_b"][0], gpt["lk_b"][1] * 0x200)

    # 7) Flash kaeru
    log("Flash kaeru")
    switch_user(dev)
    flash_binary(dev, "../bin/biscuit-kaeru.bin", gpt["expdb"][0], gpt["expdb"][1] * 0x200)

    # 8) Flash tee w/ payload to tee1
    log("Flash payload")
    switch_user(dev)
    flash_binary(dev, "../bin/tee-payload.bin", gpt["tee1"][0], gpt["tee1"][1] * 0x200)

    # 9) Downgrade preloader
    if dev.preloader:
        log("Running from the preloader, leaving the one in BOOT0 alone")
    else:
        log("Flash preloader")
        switch_boot0(dev)
        flash_binary(dev, "../bin/preloader.img", 0)

    # 10) Force fastboot
    log("Force fastboot")
    force_fastboot(dev, gpt)

    # 11) Wait some time so data is flushed to eMMC
    for _ in range(5):
        dev.kick_watchdog()
        time.sleep(1)

    # 12) Reboot (to fastboot)
    log("Reboot to unlocked fastboot")
    dev.reboot()


if __name__ == "__main__":
    args = sys.argv[1:]

    if args and args[0] in ("flash", "read"):
        if len(args) != 3:
            raise RuntimeError("{} needs a partition and a file".format(args[0]))
        args[2] = os.path.abspath(args[2])
        if args[0] == "flash" and not os.path.exists(args[2]):
            raise RuntimeError("no such file: {}".format(args[2]))
    elif args and args != ["fixgpt"]:
        raise RuntimeError("unknown command: {} (have: flash <partition> <file>, read <partition> <file>, fixgpt)".format(args[0]))

    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    dev = Device()
    dev.find_device()

    if args and args[0] == "flash":
        flash_partition(dev, args[1], args[2])
    elif args and args[0] == "read":
        read_partition(dev, args[1], args[2])
    else:
        main(dev)
