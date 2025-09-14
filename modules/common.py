import glob
import struct
import sys
import time

import serial
import serial.tools.list_ports

from logger import log

BAUD = 115200
TIMEOUT = 5
VID = "0E8D"
PID = "0003"

CRYPTO_BASE = 0x10210000  # for 6750


def serial_ports(vid=None, pid=None):
    """Lists available serial ports

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
        
        if vid and pid:
            if vid in portHwid and pid in portHwid:
                try:
                    s = serial.Serial(portDevice, timeout=TIMEOUT)
                    s.close()
                    result.add(portDevice)
                except (OSError, serial.SerialException):
                    pass
        else:
            if ('1004' in portHwid and '6000' in portHwid) or ('0E8D' in portHwid and '0003' in portHwid):
                try:
                    s = serial.Serial(portDevice, timeout=TIMEOUT)
                    s.close()
                    result.add(portDevice)
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

    def find_device(self, vid=None, pid=None):
        if self.dev:
            self.dev.close()
            self.dev = None
            
        log("Waiting for device")
        
        old = serial_ports(vid, pid)
        while True:
            new = serial_ports(vid, pid)
            
            if new > old:
                port = (new - old).pop()
                break
            elif old > new:
                old = new
                
            time.sleep(0.25)
        
        log("Found port = {}".format(port))
        
        ports = list(serial.tools.list_ports.comports())
        port_info = next((p for p in ports if p.device == port), None)
        
        self.preloader = False
        if port_info:
            if '1004' in port_info.hwid and '6000' in port_info.hwid:
                self.preloader = True
                log("Device in preloader mode")
            elif '0E8D' in port_info.hwid and '0003' in port_info.hwid:
                self.preloader = False
                log("Device in bootrom mode")
        
        self.dev = serial.Serial(port, BAUD, timeout=TIMEOUT)

    def check(self, test, gold):
        if test != gold:
            raise RuntimeError('ERROR: Serial protocol mismatch (got {}, expected {})'.format(test, gold))

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

    def crash_pl(self):
        try:
            payload = b'\x00\x01\x9F\xE5\x10\xFF\x2F\xE1' + b'\x00' * 0x110
            self.send_da(0, len(payload), 0, payload)
            self.jump_da(0)
        except RuntimeError as e:
            pass # we expect failure here

    def check_int(self, test, gold):
        test = struct.unpack('>I', test)[0]
        self.check(test, gold)

    def _writeb(self, out_str):
        self.dev.write(out_str)
        return self.dev.read()

    def handshake(self):
        for i in range(10):
            if i > 0:
                time.sleep(0.1)
            self.dev.flushInput()
            c = self._writeb(b'\xa0')
            if c == b'\x5f':
                break
        else:
            raise RuntimeError("Handshake failed")
        
        time.sleep(0.05)
        self.check(self._writeb(b'\x0a'), b'\xf5')
        time.sleep(0.05)
        self.check(self._writeb(b'\x50'), b'\xaf')
        time.sleep(0.05)
        self.check(self._writeb(b'\x05'), b'\xfa')

    def handshake2(self, cmd='FACTFACT'):
        # look for start byte
        c = 0
        while c != b'Y':
            c = self.dev.read()
        log('Preloader ready, sending ' + cmd)
        command = str.encode(cmd)
        self.dev.write(command)
        self.dev.flushInput()

    def read32(self, addr, size=1):
        result = []

        self.dev.write(b'\xd1')
        self.check(self.dev.read(1), b'\xd1')  # echo cmd

        self.dev.write(struct.pack('>I', addr))
        self.check_int(self.dev.read(4), addr)  # echo addr

        self.dev.write(struct.pack('>I', size))
        self.check_int(self.dev.read(4), size)  # echo size

        self.check(self.dev.read(2), b'\x00\x00')  # arg check

        for _ in range(size):
            data = struct.unpack('>I', self.dev.read(4))[0]
            result.append(data)

        self.check(self.dev.read(2), b'\x00\x00')  # status

        # support scalar
        if len(result) == 1:
            return result[0]
        else:
            return result

    def write32(self, addr, words, status_check=True):
        # support scalar
        if not isinstance(words, list):
            words = [words]

        self.dev.write(b'\xd4')
        self.check(self.dev.read(1), b'\xd4')  # echo cmd

        self.dev.write(struct.pack('>I', addr))
        self.check_int(self.dev.read(4), addr)  # echo addr

        self.dev.write(struct.pack('>I', len(words)))
        self.check_int(self.dev.read(4), len(words))  # echo size

        self.check(self.dev.read(2), b'\x00\x01')  # arg check

        for word in words:
            self.dev.write(struct.pack('>I', word))
            self.check_int(self.dev.read(4), word)  # echo word

        if status_check:
            self.check(self.dev.read(2), b'\x00\x01')  # status

    def run_ext_cmd(self, cmd):
        self.dev.write(b'\xc8')
        self.check(self.dev.read(1), b'\xc8')  # echo cmd
        cmd = bytes([cmd])
        self.dev.write(cmd)
        self.check(self.dev.read(1), cmd)
        self.dev.read(1)
        self.dev.read(2)

    def wait_payload(self):
        data = self.dev.read(4)
        if data != b'\xb1\xb2\xb3\xb4':
            raise RuntimeError(
                'received {} instead of expected pattern'.format(data)
            )

    def emmc_read(self, idx):
        # magic
        self.dev.write(p32_be(0xF00DD00D))
        # cmd
        self.dev.write(p32_be(0x1000))
        # block to read
        self.dev.write(p32_be(idx))

        data = self.dev.read(0x200)
        if len(data) != 0x200:
            raise RuntimeError('read fail')

        return data

    def emmc_write(self, idx, data):
        if len(data) != 0x200:
            raise RuntimeError('data must be 0x200 bytes')

        # magic
        self.dev.write(p32_be(0xF00DD00D))
        # cmd
        self.dev.write(p32_be(0x1001))
        # block to write
        self.dev.write(p32_be(idx))
        # data
        self.dev.write(data)

        code = self.dev.read(4)
        if code != b'\xd0\xd0\xd0\xd0':
            raise RuntimeError('device failure')

    def emmc_switch(self, part):
        # magic
        self.dev.write(p32_be(0xF00DD00D))
        # cmd
        self.dev.write(p32_be(0x1002))
        # partition
        self.dev.write(p32_be(part))

    def kick_watchdog(self):
        # magic
        self.dev.write(p32_be(0xF00DD00D))
        # cmd
        self.dev.write(p32_be(0x3001))

    def rpmb_read(self):
        # magic
        self.dev.write(p32_be(0xF00DD00D))
        # cmd
        self.dev.write(p32_be(0x2000))

        data = self.dev.read(0x100)
        if len(data) != 0x100:
            raise RuntimeError('read fail')

        return data

    def rpmb_write(self, data):
        if len(data) != 0x100:
            raise RuntimeError('data must be 0x100 bytes')

        # magic
        self.dev.write(p32_be(0xF00DD00D))
        # cmd
        self.dev.write(p32_be(0x2001))
        # data
        self.dev.write(data)

    def reboot(self):
        # magic
        self.dev.write(p32_be(0xf00dd00d))
        # cmd
        self.dev.write(p32_be(0x3000))        