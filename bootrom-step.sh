#!/bin/bash

set -e

python3 "$(dirname "$0")/modules/main.py" "$@"
