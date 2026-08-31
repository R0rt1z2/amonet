import struct
import sys
import glob
import time

import serial
import serial.tools.list_ports

from logger import log

BAUD = 115200
TIMEOUT = 5
VID = "0E8D"
PID = "0003"

MAX_BLOCKS_DEFAULT = 64


CRYPTO_BASE = 0x10210000


def serial_ports ():
    """ Lists available serial ports

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A set containing the serial ports available on the system
    """

    result = set()
    ports = list(serial.tools.list_ports.comports())
    for port in ports:
        if hasattr(port, 'hwid'):
            portHwid = port.hwid
            portDevice = port.device
        else:
            portHwid = port[2]
            portDevice = port[0]

        class pdev:
            hwid = portHwid
            device = portDevice
        port = pdev()

        if VID in portHwid:
            try:
                s = serial.Serial(portDevice, timeout=TIMEOUT)
                s.close()
                result.add(port)
            except (OSError, serial.SerialException):
                pass

    return result

def to_bytes(data, size = 1, endian = ">"):
    if size == 4:
        return struct.pack(endian + "I", data)
    elif size == 2:
        return struct.pack(endian + "H", data)
    else:
        return struct.pack(endian + "B", data)

def p32_be(x):
    return to_bytes(x, 4)


class Device:

    def __init__(self, port=None):
        self.dev = None
        self.preloader = False
        self.max_blocks = MAX_BLOCKS_DEFAULT
        if port:
            self.dev = serial.Serial(port, BAUD, timeout=TIMEOUT)

    def find_device(self):
        if self.dev:
            raise RuntimeError("Device already found")

        log("Waiting for device")

        old = serial_ports()
        while True:
            new = serial_ports()

            if new > old:
                port = (new - old).pop()
                break
            elif old > new:
                old = new

            time.sleep(0.25)

        log("Found port = {}".format(port.device))

        if not PID in port.hwid:
            self.preloader = True

        self.dev = serial.Serial(port.device, BAUD, timeout=TIMEOUT)

    def check(self, test, gold):
        if test != gold:
            raise RuntimeError("ERROR: Serial protocol mismatch, expected {} got {}".format(gold.hex(), test.hex()))

    def check_int(self, test, gold):
        test = struct.unpack('>I', test)[0]
        self.check(test, gold)

    def _writeb(self, out_str):
        self.dev.write(out_str)
        return self.dev.read()

    def handshake(self):
        while True:
            c = self._writeb(b'\xa0')
            if c == b'\x5f':
                self.dev.flushInput()
                self.dev.flushOutput()
                break
            self.dev.flushInput()
            self.dev.flushOutput()

        self.check(self._writeb(b'\x0a'), b'\xf5')
        self.check(self._writeb(b'\x50'), b'\xaf')
        self.check(self._writeb(b'\x05'), b'\xfa')

    def handshake2(self, cmd='FACTFACT'):
        c = 0
        while c != b'Y':
            c = self.dev.read()
        log("Preloader ready, sending " + cmd)
        command = str.encode(cmd)
        self.dev.write(command)
        self.dev.flushInput()

    def wait_payload(self):
        data = self.dev.read(4)
        if data != b"\xB1\xB2\xB3\xB4":
            raise RuntimeError("received {} instead of expected pattern".format(data))

    def command(self, cmd, *args, payload=None):
        packet = b"".join(p32_be(word) for word in (0xf00dd00d, cmd) + args)
        if payload is not None:
            packet += payload
        self.dev.write(packet)

    def read_exact(self, size):
        data = bytearray()
        while len(data) < size:
            chunk = self.dev.read(size - len(data))
            if not chunk:
                raise RuntimeError("read fail (got {} of {} bytes)".format(len(data), size))
            data += chunk
        return bytes(data)

    def expect_ack(self):
        code = self.read_exact(4)
        if code != b"\xd0\xd0\xd0\xd0":
            raise RuntimeError("device failure")

    def query_max_blocks(self):
        self.command(0x1005)
        self.max_blocks = struct.unpack('>I', self.read_exact(4))[0]
        if self.max_blocks < 1 or self.max_blocks > 0x10000:
            raise RuntimeError("device reported bogus block limit {}".format(self.max_blocks))
        return self.max_blocks

    def emmc_read(self, idx):
        self.command(0x1000, idx)
        return self.read_exact(0x200)

    def emmc_read_blocks(self, idx, blocks):
        if blocks < 1 or blocks > self.max_blocks:
            raise RuntimeError("can only read 1 to {} blocks at a time".format(self.max_blocks))

        self.command(0x1004, idx, blocks)
        return self.read_exact(blocks * 0x200)

    def emmc_write(self, idx, data):
        if len(data) != 0x200:
            raise RuntimeError("data must be 0x200 bytes")

        self.command(0x1001, idx, payload=data)
        self.expect_ack()

    def emmc_write_blocks(self, idx, data):
        if len(data) % 0x200 != 0:
            raise RuntimeError("data must be a whole number of blocks")

        blocks = len(data) // 0x200
        if blocks < 1 or blocks > self.max_blocks:
            raise RuntimeError("can only write 1 to {} blocks at a time".format(self.max_blocks))

        self.command(0x1003, idx, blocks, payload=data)
        self.expect_ack()

    def emmc_switch(self, part):
        self.command(0x1002, part)

    def reboot(self):
        self.command(0x3000)

    def kick_watchdog(self):
        self.command(0x3001)

    def rpmb_read(self):
        self.command(0x2000)
        return self.read_exact(0x100)

    def mem_read(self, address, size):
        self.command(0x5000, address, size)
        return self.read_exact(size)

    def idme_read(self, field_name):
        if len(field_name) > 16:
            raise RuntimeError("field name must be at most 16 bytes")

        self.command(0x7000, payload=field_name + b"\x00" * (16 - len(field_name)))

        size = struct.unpack('>I', self.read_exact(4))[0]
        data = self.read_exact((size + 3) & ~3)[:size]

        if data == p32_be(0xffffffff):
            raise RuntimeError("read fail")

        elif data == p32_be(0xbeefdeed):
            raise RuntimeError("IDME invalid")

        elif data == p32_be(0xdeadbeef):
            log("{} not found in IDME".format(field_name))
            return None

        return data

    def rpmb_write(self, data):
        if len(data) != 0x100:
            raise RuntimeError("data must be 0x100 bytes")

        self.command(0x2001, payload=data)

    def write(self, data, size=1):
        if type(data) != bytes:
            data = to_bytes(data, size)

        self.dev.write(data)

    def read(self, size=1):
        return self.dev.read(size)

    def echo(self, words, size=1):
        self.write(words, size)
        self.check(self.read(size), to_bytes(words, size))

    def send_da(self, da_address, da_len, sig_len, da):
        self.echo(0xD7)

        self.echo(da_address, 4)
        self.echo(da_len, 4)
        self.echo(sig_len, 4)

        self.check(self.read(2), to_bytes(0, 2))

        self.dev.write(da)

        checksum = self.dev.read(2)

        self.check(self.read(2), to_bytes(0, 2))

    def jump_da(self, da_address):
        self.echo(0xD5)

        self.echo(da_address, 4)

        self.check(self.read(2), to_bytes(0, 2))

