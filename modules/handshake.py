"""Handshake module for Amonet."""

import sys

from common import Device
from logger import log


def handshake(dev_ref):
    """Perform a handshake with the device."""
    log("Handshake")
    dev_ref.handshake()


if __name__ == "__main__":
    if len(sys.argv) > 1:
        dev = Device(sys.argv[1])
    else:
        dev = Device()
        dev.find_device()
    handshake(dev)
