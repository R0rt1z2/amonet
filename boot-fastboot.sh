#!/bin/bash

set -e

cd "$(cd "$(dirname "$0")" && pwd)"

cd modules
python3 handshake2.py FACTFACT
