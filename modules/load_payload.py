#!/usr/bin/env python3
import sys

from common import Device
from logger import log
from functions import check_modemmanager


def load_payload_file(path):
    with open(path, "rb") as fin:
        payload = fin.read()
    log("Load payload from {} = 0x{:X} bytes".format(path, len(payload)))
    while len(payload) % 4 != 0:
        payload += b"\x00"

    return payload

def load_pl_payload(dev):
    log("Handshake")
    dev.handshake()
    payload = load_payload_file("../brom-payload/pl/pl.bin")
    dev.send_da(0x40001000, len(payload), 0, payload)
    dev.jump_da(0x40001000)

    data = dev.dev.read(4)
    if data != b"\xB1\xB2\xB3\xB4":
        raise RuntimeError("received {} instead of expected pattern".format(data))

    log("All good")

if __name__ == "__main__":

    check_modemmanager()

    if len(sys.argv) > 1:
        dev = Device(sys.argv[1])
    else:
        dev = Device()
        dev.find_device()

    load_pl_payload(dev)
