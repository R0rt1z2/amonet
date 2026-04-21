"""Handshake module for Amonet."""

#!/usr/bin/env python3


import sys

from common import Device
from logger import log


def handshake2(dev_ref, cmd_ref='FACTFACT'):
    """Perform a handshake with the device."""
    log("Handshake")
    dev_ref.handshake2(cmd_ref)

if __name__ == "__main__":
    if len(sys.argv) > 2:
        dev = Device(sys.argv[2])
    else:
        dev = Device()
        dev.find_device(True)

    if len(sys.argv) > 1:
        CMD = sys.argv[1]
    else:
        CMD = 'FACTFACT'
    handshake2(dev, CMD)
