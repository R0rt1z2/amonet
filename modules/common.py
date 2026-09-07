import struct
import sys
import glob
import time

import serial
from serial.tools import list_ports

from logger import log

BAUD = 115200
TIMEOUT = 5

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

    if sys.platform.startswith("win"):
        ports = [ "COM{0:d}".format(i + 1) for i in range(256) ]
    elif sys.platform.startswith("linux"):
        ports = glob.glob("/dev/ttyACM*")
    elif sys.platform.startswith("darwin"):
        ports = glob.glob("/dev/cu.usbmodem*")
    else:
        raise EnvironmentError("Unsupported platform")

    result = set()
    for port in ports:
        try:
            s = serial.Serial(port, timeout=TIMEOUT)
            s.close()
            result.add(port)
        except (OSError, serial.SerialException):
            pass

    return result


def port_info(port):
    for info in list_ports.comports():
        if info.device == port:
            return info

    return None


def is_preloader(port):
    info = port_info(port)

    return info is not None and info.pid == 0x2000


def stock_preloader(port):
    info = port_info(port)

    return info is not None and info.pid == 0x2000 and info.manufacturer != "PWNED"


def p32_be(x):
    return struct.pack(">I", x)


class Device:

    def __init__(self, port=None, require_pwned=True):
        self.dev = None
        self.pid = None
        self.part = None
        if port:
            self.detect_mode(port, require_pwned)
            self.dev = serial.Serial(port, BAUD, timeout=TIMEOUT)

    @property
    def preloader(self):
        if self.pid == 0x2000:
            return True
        elif self.pid == 0x0003:
            return False
        else:
            return None

    def detect_mode(self, port, require_pwned=True):
        info = port_info(port)
        self.pid = info.pid if info else None

        log("Found port = {}".format(port))

        if self.preloader:
            if require_pwned and info.manufacturer != "PWNED":
                raise RuntimeError("Not in hacked USBDL mode")
            log("Device is in preloader mode")
        elif self.preloader is False:
            log("Device is in bootrom mode")
        else:
            log("Unable to determine device mode")

        return self.preloader

    def find_device(self,preloader=False):
        if self.dev:
            raise RuntimeError("Device already found")

        log("Waiting for preloader" if preloader else "Waiting for device")

        old = serial_ports()
        while True:
            new = serial_ports()

            # port added
            if new > old:
                port = (new - old).pop()
                if preloader:
                    skip = not is_preloader(port)
                else:
                    skip = stock_preloader(port)

                if skip:
                    # log("Ignoring {}".format(port))
                    old = new
                    continue
                break
            # port removed
            elif old > new:
                old = new

            time.sleep(0.25)

        self.detect_mode(port, require_pwned = not preloader)

        self.dev = serial.Serial(port, BAUD, timeout=TIMEOUT)

    def check(self, test, gold):
        if test != gold:
            raise RuntimeError("ERROR: Serial protocol mismatch")

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
                break
            self.dev.flushInput()

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

    def read32(self, addr, size=1):
        result = []

        self.dev.write(b'\xd1')
        self.check(self.dev.read(1), b'\xd1') # echo cmd

        self.dev.write(struct.pack('>I', addr))
        self.check_int(self.dev.read(4), addr) # echo addr

        self.dev.write(struct.pack('>I', size))
        self.check_int(self.dev.read(4), size) # echo size

        self.check(self.dev.read(2), b'\x00\x00') # arg check

        for _ in range(size):
            data = struct.unpack('>I', self.dev.read(4))[0]
            result.append(data)

        self.check(self.dev.read(2), b'\x00\x00') # status

        # support scalar
        if len(result) == 1:
            return result[0]
        else:
            return result

    def write32(self, addr, words, status_check=True):
        # support scalar
        if not isinstance(words, list):
            words = [ words ]

        self.dev.write(b'\xd4')
        self.check(self.dev.read(1), b'\xd4') # echo cmd

        self.dev.write(struct.pack('>I', addr))
        self.check_int(self.dev.read(4), addr) # echo addr

        self.dev.write(struct.pack('>I', len(words)))
        self.check_int(self.dev.read(4), len(words)) # echo size

        self.check(self.dev.read(2), b'\x00\x01') # arg check

        for word in words:
            self.dev.write(struct.pack('>I', word))
            self.check_int(self.dev.read(4), word) # echo word

        if status_check:
            self.check(self.dev.read(2), b'\x00\x01') # status

    def send_da(self, address, size, sig_len, da):
        self.dev.write(b'\xd7')
        self.check(self.dev.read(1), b'\xd7') # echo cmd

        self.dev.write(struct.pack('>I', address))
        self.check_int(self.dev.read(4), address) # echo address

        self.dev.write(struct.pack('>I', size))
        self.check_int(self.dev.read(4), size) # echo size

        self.dev.write(struct.pack('>I', sig_len))
        self.check_int(self.dev.read(4), sig_len) # echo sig_len

        self.check(self.dev.read(2), b'\x00\x00') # arg check

        self.dev.write(da)

        self.dev.read(2) # checksum

        self.check(self.dev.read(2), b'\x00\x00') # status

    def jump_da(self, address):
        self.dev.write(b'\xd5')
        self.check(self.dev.read(1), b'\xd5') # echo cmd

        self.dev.write(struct.pack('>I', address))
        self.check_int(self.dev.read(4), address) # echo address

        self.check(self.dev.read(2), b'\x00\x00') # status

    def run_ext_cmd(self, cmd):
        self.dev.write(b'\xC8')
        self.check(self.dev.read(1), b'\xC8') # echo cmd
        cmd = bytes([cmd])
        self.dev.write(cmd)
        self.check(self.dev.read(1), cmd)
        self.dev.read(1)
        self.dev.read(2)

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

    def emmc_read(self, idx):
        self.command(0x1000, idx)
        return self.read_exact(0x200)

    def emmc_read_blocks(self, idx, blocks):
        if blocks < 1 or blocks > BLOCKS_PER_READ:
            raise RuntimeError("can only read 1 to {} blocks at a time".format(BLOCKS_PER_READ))

        self.command(0x1004, idx, blocks)
        return self.read_exact(blocks * 0x200)

    def emmc_write(self, idx, data):
        if len(data) != 0x200:
            raise RuntimeError("data must be 0x200 bytes")

        if self.preloader and self.part == 1:
            raise RuntimeError("refusing to write to boot0 in preloader mode")

        self.command(0x1001, idx, payload=data)
        self.expect_ack()

    def emmc_write_blocks(self, idx, data):
        if len(data) % 0x200 != 0:
            raise RuntimeError("data must be a whole number of blocks")

        blocks = len(data) // 0x200
        if blocks < 1 or blocks > BLOCKS_PER_WRITE:
            raise RuntimeError("can only write 1 to {} blocks at a time".format(BLOCKS_PER_WRITE))

        if self.preloader and self.part == 1:
            raise RuntimeError("refusing to write to boot0 in preloader mode")

        self.command(0x1003, idx, blocks, payload=data)
        self.expect_ack()

    def emmc_switch(self, part):
        self.command(0x1002, part)

        self.part = part

    def mem_read(self, address, size):
        self.command(0x5000, address, size)

        # the payload pads what it sends up to a whole number of words
        return self.read_exact((size + 3) & ~3)[:size]

    def try_fast_send(self):
        expected = bytes((i * 7) & 0xFF for i in range(0x200))

        self.command(0x5002)
        try:
            got = self.read_exact(len(expected))
        except RuntimeError:
            got = None

        if got == expected:
            return True

        time.sleep(0.5)
        self.dev.flushInput()
        self.command(0x5003)
        self.expect_ack()
        return False

    def reboot(self):
        self.command(0x3000)

    def kick_watchdog(self):
        self.command(0x3001)

    def rpmb_read(self):
        self.command(0x2000)
        return self.read_exact(0x100)

    def rpmb_write(self, data):
        if len(data) != 0x100:
            raise RuntimeError("data must be 0x100 bytes")

        self.command(0x2001, payload=data)
