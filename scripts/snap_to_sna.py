#!/usr/bin/env python3
"""Convert one of our RAM+register snapshots into a 48K .sna file
ZEsarUX can load via snapshot-load.

.SNA layout (48K):
    00      I
    01..08  HL', DE', BC', AF'
    09..12  HL, DE, BC
    0F..10  IY
    11..12  IX
    13      IFF2 (bit 2)
    14      R
    15..16  AF
    17..18  SP
    19      IM
    1A      border colour
    1B..    48 KB RAM (0x4000..0xFFFF)

When ZEsarUX loads a .sna, it RETIs from the saved IFF state and pops
PC from (SP). So we must push our snapshot's PC onto the stack before
serialising: decrement SP by 2 and write PC there.
"""
import argparse
import ast
import sys
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('snap_dir')
    ap.add_argument('out')
    args = ap.parse_args()

    snap = Path(args.snap_dir)
    ram = bytearray((snap / 'ram_4000_FFFF.bin').read_bytes())
    assert len(ram) == 49152, f"expected 48K RAM, got {len(ram)}"
    regs = ast.literal_eval((snap / 'registers.txt').read_text().strip())

    pc = regs['PC']
    sp = regs['SP']
    # Push PC onto the stack: SP -= 2, write PC there.
    sp_new = (sp - 2) & 0xFFFF
    if sp_new < 0x4000:
        print(f"WARN: SP={sp:04X} too low to push PC; result file may misload",
              file=sys.stderr)
    # RAM offset of sp_new
    ram[(sp_new - 0x4000) + 0] = pc & 0xFF
    ram[(sp_new - 0x4000) + 1] = (pc >> 8) & 0xFF

    def w16(v): return bytes([v & 0xFF, (v >> 8) & 0xFF])

    hdr = bytearray(27)
    hdr[0x00:0x01] = bytes([regs['I'] & 0xFF])
    hdr[0x01:0x03] = w16(regs.get("HL'", 0))
    hdr[0x03:0x05] = w16(regs.get("DE'", 0))
    hdr[0x05:0x07] = w16(regs.get("BC'", 0))
    hdr[0x07:0x09] = w16(regs.get("AF'", 0))
    hdr[0x09:0x0B] = w16(regs['HL'])
    hdr[0x0B:0x0D] = w16(regs['DE'])
    hdr[0x0D:0x0F] = w16(regs['BC'])
    hdr[0x0F:0x11] = w16(regs.get('IY', 0))
    hdr[0x11:0x13] = w16(regs.get('IX', 0))
    hdr[0x13] = 0x04                 # IFF2 set (interrupts enabled) — best guess
    hdr[0x14] = regs.get('R', 0) & 0xFF
    hdr[0x15:0x17] = w16(regs['AF'])
    hdr[0x17:0x19] = w16(sp_new)     # SP after PC push
    hdr[0x19] = 1                    # IM 1 — best guess
    hdr[0x1A] = 0                    # border colour 0

    out = Path(args.out)
    out.write_bytes(bytes(hdr) + bytes(ram))
    print(f"wrote {out} ({len(hdr)+len(ram)} B), PC=0x{pc:04X} pushed at SP=0x{sp_new:04X}")


if __name__ == '__main__':
    main()
