import struct
import sys
import glob
import time

import serial
import serial.tools.list_ports

from logger import log

BAUD = 115200
TIMEOUT = 5


CRYPTO_BASE = 0x10210000 # for suez


def serial_ports (vid="0E8D", pid=""):
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
        if vid in portHwid and pid in portHwid:
            try:
                s = serial.Serial(portDevice, timeout=TIMEOUT)
                s.close()
                result.add(portDevice)
            except (OSError, serial.SerialException):
                pass

    return result


BLOCKS_PER_WRITE = 64


def port_info(port):
    for info in serial.tools.list_ports.comports():
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
            print(test)
            print(gold)
            #print("ERROR: Serial protocol mismatch")
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

    def emmc_write(self, idx, data):
        if len(data) != 0x200:
            raise RuntimeError("data must be 0x200 bytes")

        if self.preloader and self.part == 1:
            raise RuntimeError("refusing to write to boot0 in preloader mode")

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

        if self.preloader and self.part == 1:
            raise RuntimeError("refusing to write to boot0 in preloader mode")

        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x1003))
        # block to write
        self.dev.write(p32_be(idx))
        self.dev.write(p32_be(blocks))
        # data
        self.dev.write(data)

        code = self.dev.read(4)
        if code != b"\xd0\xd0\xd0\xd0":
            raise RuntimeError("device failure")

    def mem_read(self, address, size):
        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x5000))
        # address
        self.dev.write(p32_be(address))
        # size
        self.dev.write(p32_be(size))

        # the payload pads what it sends up to a whole number of words
        data = self.dev.read((size + 3) & ~3)
        if len(data) != ((size + 3) & ~3):
            raise RuntimeError("read fail")

        return data[:size]

    def idme_read(self, field_name):
        if len(field_name) > 16:
            raise RuntimeError("field name must be at most 16 bytes")

        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x7000))
        # field to read
        self.dev.write(field_name + b"\x00" * (16 - len(field_name)))

        self.part = 0

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

    def emmc_switch(self, part):
        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x1002))
        # partition
        self.dev.write(p32_be(part))

        self.part = part

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

    def rpmb_write(self, data):
        if len(data) != 0x100:
            raise RuntimeError("data must be 0x100 bytes")

        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x2001))
        # data
        self.dev.write(data)
