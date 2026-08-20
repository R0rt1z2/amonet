#!/usr/bin/env python3

import sys
import struct
import uuid
import zlib

from collections import namedtuple
from io import BufferedReader

from logger import log

SIGNATURE = b'EFI PART'
PRIMARY_GPT_LBA = 1
BLOCK_SIZE = 0x200
ALIGN_BLOCKS = 0x400
HEADER_SIZE = 0x5C
PART_SIZE = 0x80
PART_NUM = 0x80

HEADER_STRUCT = '<8sIIIIQQQQ16sQIII420x'
PART_STRUCT = '<16s16sQQQ72s'

def get_sectors(gpt_data, lba, count):
    sector = b''

    if isinstance(gpt_data, BufferedReader):
        if lba < 0:
            gpt_data.seek(BLOCK_SIZE*lba, 2)
        else:
            gpt_data.seek(BLOCK_SIZE*lba)

        sector = gpt_data.read(BLOCK_SIZE*count)
    else:
        if lba < 0:
            start = len(gpt_data)+(BLOCK_SIZE*lba)
        else:
            start = BLOCK_SIZE*lba
        sector = gpt_data[start:start+(BLOCK_SIZE*count)] 

    return sector

def get_part_table(gpt_data, gpt_header):
    if gpt_header['part_lba'] != 2:
        start_lba = - (((gpt_header['part_size'] * gpt_header['part_num']) // BLOCK_SIZE) + 1)
    else:
        start_lba = gpt_header['part_lba']
    part_table = get_sectors(gpt_data, start_lba, ((gpt_header['part_size'] * gpt_header['part_num']) // BLOCK_SIZE))
    
    return part_table

def calc_header_crc32(sector, header_size):
    return zlib.crc32(sector[:0x10] + b'\x00\x00\x00\x00' + sector[0x10 + 4:header_size])

def parse_header(gpt_data, lba):
    sector = get_sectors(gpt_data, lba, 1)
    
    gpt_header_t = namedtuple('gpt_header', 'signature version header_size header_crc32 reserved this_lba other_lba first_lba last_lba guid part_lba part_num part_size part_crc32')
    gpt_header = gpt_header_t(*struct.unpack(HEADER_STRUCT, sector[:512]))._asdict()
    
    assert gpt_header["signature"] == SIGNATURE, "No valid GPT header found"

    header_crc32 = calc_header_crc32(sector, gpt_header['header_size'])
    assert header_crc32 == gpt_header['header_crc32'], "GPT header is corrupt, expected CRC32 %.8X but got %.8X" % (header_crc32, gpt_header["header_crc32"])

    log('')
    log('Sector size (logical): %s bytes'%BLOCK_SIZE)
    log('Disk identifier (GUID): %s'%str(uuid.UUID(bytes_le=gpt_header['guid'])).upper())
    log('Partition table holds up to %d entries'%gpt_header['part_num'])
    log('This partition table begins at sector %d and ends at sector %d'%(gpt_header['part_lba'], gpt_header['part_lba'] - 1 + (gpt_header['part_num'] * gpt_header['part_size'] / BLOCK_SIZE))) 
    log('First usable sector is %d, last usable sector is %d'%(gpt_header['first_lba'], gpt_header['last_lba']))
    log('Other partition table is at sector %d'%gpt_header['other_lba'])
    log('')
    
    return gpt_header

def parse_partition(part_table, offset, size):
    partition_t = namedtuple('partition', 'type_guid guid start end attrib name')
    partition = partition_t(*struct.unpack(PART_STRUCT, part_table[offset:offset+size]))._asdict()
    return partition

def parse_part_table(part_table, gpt_header):
    
    part_list = []
   
    part_table += b'\x00' * ((gpt_header['part_num'] * gpt_header['part_size']) - len(part_table))

    part_crc32 = zlib.crc32(part_table)
    assert part_crc32 == gpt_header['part_crc32'], "Partition table is corrupt, expected CRC32 %.8X but got %.8X" % (part_crc32, gpt_header["part_crc32"])

    log("{:<5}  {:>15}  {:>15}  {:<12}  {:<15} ".format('Number', 'Start (sector)', 'End (sector)', 'Size', 'Name'))
    for partition_num in range(0, gpt_header['part_num']):
        partition = parse_partition(part_table, gpt_header['part_size'] * partition_num, gpt_header['part_size'])
        
        # invalid/empty
        if partition['end'] == 0:
            continue
        
        part_list.append(partition)
        name = partition['name'].decode("utf-16le").rstrip("\x00")
        part_size = (partition['end'] - partition['start'] + 1) * BLOCK_SIZE
        units = ["B", "KiB", "MiB", "GiB"]
        unit = 0
        while part_size > 0x400:
            part_size /= 0x400
            unit += 1
        size = "{:.2f} {:s}".format(part_size, units[unit])
        log("{:>5}  {:>15}  {:>15}  {:<12s}  {:<15} ".format(partition_num + 1, partition['start'], partition['end'], size, name))

    log('')
    
    return part_list

def get_part_by_name(part_list, name):
    for part in part_list:
        try:
            if part['name'].decode("utf-16le").rstrip("\x00") == name:
                return part
        except:
            continue
    return None


def parse_gpt(gpt_data):
    try:
        gpt_header = parse_header(gpt_data, PRIMARY_GPT_LBA)
    except:
        log("No valid primary GPT header found, looking for backup GPT")
        try:
            gpt_header = parse_header(gpt_data, -1)
        except:
            log("No valid backup GPT found")
            raise LookupError("No valid GPT found")

    part_table = get_part_table(gpt_data, gpt_header)
    part_list = parse_part_table(part_table, gpt_header)
    return gpt_header, part_list

def parse_gpt_compat(gpt_data):
    gpt_header, part_list = parse_gpt((b'\x00' * BLOCK_SIZE) + gpt_data)
    parts = dict()
    for part in part_list:
        parts[part["name"].decode("utf-16le").strip("\x00")] = ( part["start"], part["end"] - part["start"] + 1)

    return parts, gpt_header, part_list

def gen_mbr():
    mbr = bytearray(b'\x00' * BLOCK_SIZE)
    mbr[0x1c0:0x1d0]=bytes.fromhex('0200eeffffff01000000ffffffff0000')
    mbr[0x1f0:0x200]=bytes.fromhex('000000000000000000000000000055aa')
    return mbr

def create_header(this_lba, other_lba, last_lba, guid, part_lba, part_crc32):
    gpt_header = struct.pack(HEADER_STRUCT, SIGNATURE, 0x010000, HEADER_SIZE, 0x00,         0x00, this_lba, other_lba, 0x22, last_lba, guid, part_lba, PART_NUM, PART_SIZE, part_crc32)
    header_crc32 = calc_header_crc32(gpt_header, HEADER_SIZE)
    gpt_header = struct.pack(HEADER_STRUCT, SIGNATURE, 0x010000, HEADER_SIZE, header_crc32, 0x00, this_lba, other_lba, 0x22, last_lba, guid, part_lba, PART_NUM, PART_SIZE, part_crc32)
    return gpt_header

def create_part_table(part_list, part_num=PART_NUM):
    part_table = b'' 
    for part in part_list:
        part_table += struct.pack(PART_STRUCT, part['type_guid'], part['guid'], part['start'], part['end'], part['attrib'], part['name'])
    part_table += b'\x00' * ((part_num * PART_SIZE) - len(part_table))
    return part_table

def generate_gpt(gpt_header, part_list):
    mbr = gen_mbr()

    part_table = create_part_table(part_list)
    part_crc32 = zlib.crc32(part_table)
    part_blocks = ((gpt_header['part_size'] * gpt_header['part_num']) // BLOCK_SIZE) + 1
    primary = mbr + create_header(PRIMARY_GPT_LBA, gpt_header['last_lba'] + part_blocks, gpt_header['last_lba'], gpt_header['guid'], 2, part_crc32) + part_table
    backup = part_table + create_header(gpt_header['last_lba'] + part_blocks, 1, gpt_header['last_lba'], gpt_header['guid'], gpt_header['last_lba'] + 1, part_crc32)
    return primary, backup

def modify_step1(part_list):
    part_list_n = part_list.copy()
    part_n = len(part_list) - 1
    partition = part_list_n[len(part_list_n) - 1]

    assert partition['name'].decode("utf-16le").rstrip("\x00") == "userdata", "the last partition is not userdata, refusing modification"

    partition['end'] = ((partition['end'] // ALIGN_BLOCKS) * ALIGN_BLOCKS) - 0x6E000 - 1
    
    partition_n = partition.copy()
    partition_n['guid'] = uuid.uuid4().bytes_le
    partition_n['start'] = partition['end'] + 1
    partition_n['end'] = partition_n['start'] + 0x37000 - 1
    partition_n['name'] = "boot_tmp".encode("utf-16le") + b"\x00\x00"
    part_list_n.append(partition_n)

    partition_n = partition_n.copy()
    partition_n['guid'] = uuid.uuid4().bytes_le
    partition_n['start'] = partition_n['end'] + 1
    partition_n['end'] = partition_n['start'] + 0x37000 - 1
    partition_n['name'] = "recovery_tmp".encode("utf-16le") + b"\x00\x00"
    part_list_n.append(partition_n)

    return part_list_n

def modify_step2(part_list):

    part_list_n = part_list.copy()
    partition = get_part_by_name(part_list_n, "boot")
    if partition:
        partition["name"] = "boot_x".encode("utf-16le") + b"\x00\x00"

    partition = get_part_by_name(part_list_n, "recovery")
    if partition:
        partition["name"] = "recovery_x".encode("utf-16le") + b"\x00\x00"

    partition = get_part_by_name(part_list_n, "boot_tmp")
    if partition:
        partition["name"] = "boot".encode("utf-16le") + b"\x00\x00"

    partition = get_part_by_name(part_list_n, "recovery_tmp")
    if partition:
        partition["name"] = "recovery".encode("utf-16le") + b"\x00\x00"

    return part_list_n

def unpatch(gpt_header, part_list):
    part_list_n = part_list.copy()
    part_n = len(part_list) - 1
    partition = part_list_n[len(part_list_n) - 3]

    assert partition['name'].decode("utf-16le").rstrip("\x00") == "userdata", "userdata is not where it is expected, refusing to unpatch"

    partition['end'] =  gpt_header["last_lba"]
    part_list_n[len(part_list_n) - 2]  = {'type_guid':b"\x00", 'guid':b'\x00', 'start':0, 'end':0, 'attrib':0, 'name':b'\x00'}
    part_list_n[len(part_list_n) - 1]  = {'type_guid':b"\x00", 'guid':b'\x00', 'start':0, 'end':0, 'attrib':0, 'name':b'\x00'}

    partition = get_part_by_name(part_list_n, "boot_x")
    if partition:
        partition["name"] = "boot".encode("utf-16le") + b"\x00\x00"

    partition = get_part_by_name(part_list_n, "recovery_x")
    if partition:
        partition["name"] = "recovery".encode("utf-16le") + b"\x00\x00"
    return part_list_n

def main():
    
    if len(sys.argv) == 2:
        cmd = "print"
        in_file = sys.argv[1]
    elif len(sys.argv) == 3:
        cmd = sys.argv[1]
        in_file = sys.argv[2]
    else:
        print("Usage: " + sys.argv[0]  + " [ print | patch | unpatch ] <filename>")
        sys.exit()
    
    f = open(in_file, 'rb')

    log("Input GPT:")
    
    gpt_header, part_list = parse_gpt(f)

    log("Regenerate primary and backup GPT from input")
    primary, backup = generate_gpt(gpt_header, part_list)

    log("Writing regenerated GPT to " + in_file + ".gpt") 
    with open(in_file + '.gpt', 'wb') as fout:
        fout.write(primary)

    log("Writing regenerated backup GPT to " + in_file + ".bak") 
    with open(in_file + '.bak', 'wb') as fout:
        fout.write(backup)

    log("Writing backup GPT offset to " + in_file + ".offset") 
    with open(in_file + '.offset', 'w') as fout:
        fout.write("{}\n".format(gpt_header['last_lba'] + 1))

    if cmd == "patch":

        part_list_mod1 = modify_step1(part_list)
        primary_step1, backup_step1 = generate_gpt(gpt_header, part_list_mod1)

        log('')
        log("Modified GPT Step 1:")
        gpt_header, part_list = parse_gpt(bytes(primary_step1))

        log("Writing primary GPT (part 1) to " + in_file + ".step1.gpt") 
        with open(in_file + '.step1.gpt', 'wb') as fout:
            fout.write(primary_step1)
        log("Writing backup GPT (part 1) to " + in_file + ".step1.bak") 
        with open(in_file + '.step1.bak', 'wb') as fout:
            fout.write(backup_step1)

        part_list_mod2 = modify_step2(part_list_mod1)
        primary_step2, backup_step2 = generate_gpt(gpt_header, part_list_mod2)

        log('')
        log("Modified GPT Step 2:")
        gpt_header, part_list = parse_gpt(bytes(primary_step2))

        log("Writing primary GPT (part 2) to " + in_file + ".step2.gpt") 
        with open(in_file + '.step2.gpt', 'wb') as fout:
            fout.write(primary_step2)
        log("Writing backup GPT (part 2) to " + in_file + ".step2.bak") 
        with open(in_file + '.step2.bak', 'wb') as fout:
            fout.write(backup_step2)

    elif cmd == "unpatch":
        part_list_unpatched = unpatch(gpt_header, part_list)
        primary_unpatched, backup_unpatched = generate_gpt(gpt_header, part_list_unpatched)

        log('')
        log("Unpatched GPT:")
        gpt_header, part_list = parse_gpt(bytes(primary_unpatched))

        log("Writing primary GPT (unpatched) to " + in_file + ".unpatched.gpt") 
        with open(in_file + '.unpatched.gpt', 'wb') as fout:
            fout.write(primary_unpatched)
        log("Writing backup GPT (unpatched) to " + in_file + ".unpatched.bak") 
        with open(in_file + '.unpatched.bak', 'wb') as fout:
            fout.write(backup_unpatched)

    f.close()
    
if __name__ == "__main__":
    main()
