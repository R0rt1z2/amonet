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

BLOCKS_PER_WRITE = 64
BLOCKS_PER_READ = 64


CRYPTO_BASE = 0x10210000 # for karnak


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
        if port:
            self.dev = serial.Serial(port, BAUD, timeout=TIMEOUT)

    def find_device(self):
        if self.dev:
            raise RuntimeError("Device already found")

        log("Waiting for device")

        old = serial_ports()
        while True:
            new = serial_ports()

            # port added
            if new > old:
                port = (new - old).pop()
                break
            # port removed
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
        # look for start byte
        while True:
            c = self._writeb(b'\xa0')
            if c == b'\x5f':
                self.dev.flushInput()
                self.dev.flushOutput()
                break
            self.dev.flushInput()
            self.dev.flushOutput()

        # complete sequence
        self.check(self._writeb(b'\x0a'), b'\xf5')
        self.check(self._writeb(b'\x50'), b'\xaf')
        self.check(self._writeb(b'\x05'), b'\xfa')

    def handshake2(self, cmd='FACTFACT'):
        # look for start byte
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

    def emmc_read(self, idx):
        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x1000))
        # block to read
        self.dev.write(p32_be(idx))

        data = self.dev.read(0x200)
        if len(data) != 0x200:
            raise RuntimeError("read fail")

        return data

    def emmc_read_blocks(self, idx, blocks):
        if blocks < 1 or blocks > BLOCKS_PER_READ:
            raise RuntimeError("can only read 1 to {} blocks at a time".format(BLOCKS_PER_READ))

        self.dev.write(p32_be(0xf00dd00d))
        self.dev.write(p32_be(0x1004))
        self.dev.write(p32_be(idx))
        self.dev.write(p32_be(blocks))

        size = blocks * 0x200
        data = b""
        while len(data) < size:
            chunk = self.dev.read(size - len(data))
            if not chunk:
                raise RuntimeError("read fail (got {} of {} bytes)".format(len(data), size))
            data += chunk

        return data

    def emmc_write(self, idx, data):
        if len(data) != 0x200:
            raise RuntimeError("data must be 0x200 bytes")

        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x1001))
        # block to write
        self.dev.write(p32_be(idx))
        # data
        self.dev.write(data)

        code = self.dev.read(4)
        if code != b"\xd0\xd0\xd0\xd0":
            raise RuntimeError("device failure")

    def emmc_write_blocks(self, idx, data):
        if len(data) % 0x200 != 0:
            raise RuntimeError("data must be a whole number of blocks")

        blocks = len(data) // 0x200
        if blocks < 1 or blocks > BLOCKS_PER_WRITE:
            raise RuntimeError("can only write 1 to {} blocks at a time".format(BLOCKS_PER_WRITE))

        self.dev.write(p32_be(0xf00dd00d))
        self.dev.write(p32_be(0x1003))
        self.dev.write(p32_be(idx))
        self.dev.write(p32_be(blocks))
        self.dev.write(data)

        code = self.dev.read(4)
        if code != b"\xd0\xd0\xd0\xd0":
            raise RuntimeError("device failure")

    def emmc_switch(self, part):
        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x1002))
        # partition
        self.dev.write(p32_be(part))

    def reboot(self):
        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x3000))        

    def kick_watchdog(self):
        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x3001))

    def rpmb_read(self):
        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x2000))

        data = self.dev.read(0x100)
        if len(data) != 0x100:
            raise RuntimeError("read fail")

        return data

    def mem_read(self, address, size):
        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x5000))
        # address
        self.dev.write(p32_be(address))
        # size
        self.dev.write(p32_be(size))

        data = self.dev.read(size)
        if len(data) != size:
            raise RuntimeError("read fail")

        return data

    def idme_read(self, field_name):
        if len(field_name) > 16:
            raise RuntimeError("field name must be at most 16 bytes")

        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x7000))
        self.dev.write(field_name + b"\x00" * (16 - len(field_name)))

        size = struct.unpack('>I', self.dev.read(4))[0]

        data = self.dev.read((size + 3) & ~3)
        if len(data) != ((size + 3) & ~3):
            raise RuntimeError("read fail")
        data = data[:size]

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

        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x2001))
        # data
        self.dev.write(data)

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

