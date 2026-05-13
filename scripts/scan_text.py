#!/usr/bin/env python3
"""Scan a RAM dump for printable ASCII / near-ASCII runs.

Looks for:
  - plain runs of >=4 printable chars [0x20..0x7E]
  - runs ending in a high-bit-set terminator byte (ZX-style end-of-string)
"""
import sys
from pathlib import Path

RAM_BASE = 0x4000
MIN_RUN  = 4


def scan(data: bytes, base: int):
    runs, i, n = [], 0, len(data)
    while i < n:
        if 0x20 <= data[i] <= 0x7E:
            j = i
            while j < n and 0x20 <= data[j] <= 0x7E:
                j += 1
            length = j - i
            term = j < n and 0xA0 <= data[j] <= 0xFE
            if term:
                length += 1
            if (j - i) >= MIN_RUN:
                txt = data[i:j].decode("latin-1")
                runs.append((base + i, length, txt, term))
            i = j + (1 if term else 0)
        else:
            i += 1
    return runs


def main():
    p = Path(sys.argv[1])
    data = p.read_bytes()
    runs = scan(data, RAM_BASE)
    print(f"# {p.parent.name}  ({len(runs)} runs)")
    for addr, length, txt, term in runs:
        tag = "+T" if term else "  "
        print(f"  0x{addr:04X}  len={length:>3}  {tag}  {txt!r}")


if __name__ == "__main__":
    main()
