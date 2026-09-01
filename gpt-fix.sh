#!/bin/bash

set -e

cd "$(cd "$(dirname "$0")" && pwd)"

cd modules
python3 main.py fixgpt
