"""Module to load a payload onto the device using the crypto engine.
This is used to load the payload onto the device, and then jump to it."""

import struct
import time
import threading
import sys
from common import CRYPTO_BASE, Device
from logger import log

def init(dev_ref):
    """Initialize the crypto engine by zeroing out all the relevant registers and buffers."""
    dev_ref.write32(CRYPTO_BASE + 0x0C0C, 0)
    dev_ref.write32(CRYPTO_BASE + 0x0C10, 0)
    dev_ref.write32(CRYPTO_BASE + 0x0C14, 0)
    dev_ref.write32(CRYPTO_BASE + 0x0C18, 0)
    dev_ref.write32(CRYPTO_BASE + 0x0C1C, 0)
    dev_ref.write32(CRYPTO_BASE + 0x0C20, 0)
    dev_ref.write32(CRYPTO_BASE + 0x0C24, 0)
    dev_ref.write32(CRYPTO_BASE + 0x0C28, 0)
    dev_ref.write32(CRYPTO_BASE + 0x0C2C, 0)
    dev_ref.write32(CRYPTO_BASE + 0x0C00 + 18 * 4, [0] * 4)
    dev_ref.write32(CRYPTO_BASE + 0x0C00 + 22 * 4, [0] * 4)
    dev_ref.write32(CRYPTO_BASE + 0x0C00 + 26 * 4, [0] * 8)


def hw_acquire(dev_ref):
    """Acquire the crypto engine hardware by writing to the relevant registers."""
    dev_ref.write32(CRYPTO_BASE, [0x1F, 0x12000])


def call_func(dev_ref, func):
    """Call a function in the crypto engine by writing
    to the relevant registers and waiting for the result."""
    dev_ref.write32(CRYPTO_BASE + 0x0804, 3)
    dev_ref.write32(CRYPTO_BASE + 0x0808, 3)
    dev_ref.write32(CRYPTO_BASE + 0x0C00, func)
    dev_ref.write32(CRYPTO_BASE + 0x0400, 0)
    while not dev_ref.read32(CRYPTO_BASE + 0x0800):
        pass
    if dev_ref.read32(CRYPTO_BASE + 0x0800) & 2 == 0:
        if not dev_ref.read32(CRYPTO_BASE + 0x0800) & 1:
            while not dev_ref.read32(CRYPTO_BASE + 0x0800) & 1:
                pass
        result = -1
        dev_ref.write32(CRYPTO_BASE + 0x0804, 3)
    else:
        while not dev_ref.read32(CRYPTO_BASE + 0x0418) & 1:
            pass
        result = 0
        dev_ref.write32(CRYPTO_BASE + 0x0804, 3)
    return result


def aes_write16(dev_ref, addr, data):
    """Write 16 bytes of data to the crypto engine's internal buffer,
    which is used for AES operations."""
    if len(data) != 16:
        raise RuntimeError("data must be 16 bytes")

    pattern = bytes.fromhex("4dd12bdf0ec7d26c482490b3482a1b1f")

    # iv-xor
    words = []
    for x in range(4):
        word = data[x*4:(x+1)*4]
        word = struct.unpack("<I", word)[0]
        pat = struct.unpack("<I", pattern[x*4:(x+1)*4])[0]
        words.append(word ^ pat)

    dev_ref.write32(CRYPTO_BASE + 0xC00 + 18 * 4, [0] * 4)
    dev_ref.write32(CRYPTO_BASE + 0xC00 + 22 * 4, [0] * 4)
    dev_ref.write32(CRYPTO_BASE + 0xC00 + 26 * 4, [0] * 8)

    dev_ref.write32(CRYPTO_BASE + 0xC00 + 26 * 4, words)

    dev_ref.write32(CRYPTO_BASE + 0xC04, 0) # src to VALID address which has all zeroes (otherwise, update pattern)
    dev_ref.write32(CRYPTO_BASE + 0xC08, addr) # dst to our destination
    dev_ref.write32(CRYPTO_BASE + 0xC0C, 1)
    dev_ref.write32(CRYPTO_BASE + 0xC14, 18)
    dev_ref.write32(CRYPTO_BASE + 0xC18, 26)
    dev_ref.write32(CRYPTO_BASE + 0xC1C, 26)
    if call_func(dev_ref, 126) != 0: # aes decrypt
        raise RuntimeError("failed to call the function!")


class UserInputThread(threading.Thread):
    """Thread to wait for user input while keeping the main thread alive to kick the watchdog."""
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


def load_payload(dev_ref, path):
    """Load a payload onto the device using the crypto engine.
    This involves initializing the crypto engine,
    disabling caches and bootrom range checks,
    writing the payload to memory, and then jumping to it."""
    thread = UserInputThread()
    thread.start()
    while not thread.done:
        dev_ref.write32(0x10007008, 0x1971) # low-level watchdog kick
        time.sleep(1)

    log("Init crypto engine")
    init(dev_ref)
    hw_acquire(dev_ref)
    init(dev_ref)
    hw_acquire(dev_ref)

    log("Disable caches")
    dev_ref.run_ext_cmd(0xB1)

    log("Disable bootrom range checks")
    aes_write16(dev_ref, 0x102868, bytes.fromhex("00000000000000000000000080000000"))

    with open(path, "rb") as fin:
        payload = fin.read()
    log(f"Load payload from {path} = 0x{len(payload):X} bytes")
    while len(payload) % 4 != 0:
        payload += b"\x00"

    words = []
    for x in range(len(payload) // 4):
        word = payload[x*4:(x+1)*4]
        word = struct.unpack("<I", word)[0]
        words.append(word)

    log("Send payload")
    dev_ref.write32(0x201000, words)

    log("Let's rock")
    dev_ref.write32(0x1028A8, 0x201000, status_check=False)

    log("Wait for the payload to come online...")
    dev_ref.wait_payload()
    log("all good")


if __name__ == "__main__":
    dev = Device(sys.argv[1])
    load_payload(dev, sys.argv[2])
