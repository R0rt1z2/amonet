#!/bin/bash

set -e

cd modules
python3 main.py -g --size 8
cd ..