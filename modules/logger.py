"""Logging utilities for Amonet."""

import datetime

def log(s):
    """Log a message to the console and to the log file."""
    line = f"[{datetime.datetime.now()}] {s}"
    print(line)

    with open("amonet.log", "a", encoding="utf-8") as fout:
        fout.write(line + "\n")
