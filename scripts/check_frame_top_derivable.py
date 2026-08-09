#!/usr/bin/env python3
"""The frame's top strip is eight tape sprites, not captured pixels.

`assets/frame_l1.bin` is the perimeter ornament, extracted from emulator
screens (`scripts/extract_frame.py`), and PLAN.md WS7 item 1 wants it
replaced by a port of the `LBE8B` compositor. This gate is the evidence
that the compositor's top arm is reproducible: it lays out the eight
sprites `set_border_horizontal` names and asserts the captured bytes
agree, for all four colour cycles.

    set_border_horizontal:
      DEFW spr_bord_horiz_left_edge   / left_thin / left_bold / left_thin
      DEFW spr_bord_horiz_right_thin  / right_bold / right_thin / right_edge

    LD HL,set_border_horizontal / EXX / LD HL,$0700
    LBE8B_6:
      EXX / LD E,(HL) / INC HL / LD D,(HL) / INC HL / PUSH DE
      EXX / POP DE / CALL print_sprite_pix / CALL print_sprite_attrib
      LD A,$20 / ADD A,L / LD L,A / JR NC,LBE8B_6

so eight 4-byte-wide sprites at x = 0, $20, $40 ... $E0, y = $07.

### print_sprite_pix draws UPWARD, and that is the whole trick

    print_sprite_pix_2:
      LD A,(DE) / LD (HL),A / INC DE / INC L / DJNZ print_sprite_pix_2
      DEC L / LD A,L
    sprite_next_line:
      SUB $00            ; patched with width + $1F
      LD L,A / JP NC,... / DEC H

It is a plain unmasked copy — no mask, no OR — and each row moves to the
PREVIOUS buffer line. So the first data row lands at the given y and
later rows stack ABOVE it: a sprite at y=$07 occupies y=0..7 with its
rows reversed.

Laying the same data out top-down instead matches 58 bytes of 256. That
was the first attempt here, and it looked like the strip simply was not
derivable.

### Scope

The top strip only: y 0..7, all 32 byte-columns, 256 bytes per cycle,
1024 of `frame_l1.bin`'s 4968. The side strips and the attribute rows
are the rest of `LBE8B` and are not covered.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GFX = ROOT / "original/disasm/gfx/sprites_color_1.asm"
FRAME = ROOT / "assets/frame_l1.bin"

# set_border_horizontal's eight entries, in order.
SEQUENCE = [
    "spr_bord_horiz_left_edge",
    "spr_bord_horiz_left_thin",
    "spr_bord_horiz_left_bold",
    "spr_bord_horiz_left_thin",
    "spr_bord_horiz_right_thin",
    "spr_bord_horiz_right_bold",
    "spr_bord_horiz_right_thin",
    "spr_bord_horiz_right_edge",
]
BASE_Y = 0x07          # LD HL,$0700 — H is the y, L the x
N_CYCLES = 4


def sprite(asm: str, name: str):
    """(width, height, pixel bytes) straight from the disassembly.

    Parsed, not transcribed: a gate carrying its own copy of the bytes
    agrees with a wrong copy in the port as long as both are wrong the
    same way.
    """
    try:
        i = asm.index(name + ":")
    except ValueError:
        raise SystemExit(f"FAIL: {name} is not in {GFX.name} — if the "
                         f"border sprites moved, point this gate at them")
    nxt = asm.find("; Data block", i)
    body = asm[i:nxt if nxt > 0 else len(asm)]
    b = [int(h, 16) for h in re.findall(r"\$([0-9A-Fa-f]{2})", body)]
    w, h = b[0], b[1]
    if len(b) < 2 + w * h:
        raise SystemExit(f"FAIL: {name} declares {w}x{h} but carries "
                         f"{len(b) - 2} bytes")
    return w, h, b[2:2 + w * h]


def main() -> int:
    if not FRAME.exists():
        raise SystemExit("FAIL: assets/frame_l1.bin is missing")
    asm = GFX.read_text()
    sprites = [sprite(asm, n) for n in SEQUENCE]

    frame = FRAME.read_bytes()
    if len(frame) % N_CYCLES:
        raise SystemExit(f"FAIL: frame_l1.bin is {len(frame)} bytes, not a "
                         f"multiple of {N_CYCLES} cycles")
    per = len(frame) // N_CYCLES

    checked = 0
    bad = []
    for cyc in range(N_CYCLES):
        x_byte = 0
        for name, (w, h, px) in zip(SEQUENCE, sprites):
            for row in range(h):
                y = BASE_Y - row               # print_sprite_pix goes UP
                for bx in range(w):
                    want = px[row * w + bx]
                    got = frame[cyc * per + y * 32 + x_byte + bx]
                    checked += 1
                    if got != want:
                        bad.append((cyc, name, row, bx, want, got))
            x_byte += w

    if bad:
        print(f"FAIL: {len(bad)} of {checked} top-strip bytes do not match "
              f"the tape's sprites.\n")
        for cyc, name, row, bx, want, got in bad[:12]:
            print(f"  cycle {cyc} {name} row {row} byte {bx}: "
                  f"want {want:02X} got {got:02X}")
        if len(bad) > 12:
            print(f"  ... and {len(bad) - 12} more")
        print("\nEither set_border_horizontal's order changed, or "
              "frame_l1.bin was re-extracted from a different capture. "
              "Note the rows go UPWARD from y=$07 — laying them out "
              "top-down matches 58 of 256.")
        return 1

    print(f"PASS frame_top_derivable: all {checked} top-strip bytes "
          f"({N_CYCLES} cycles x 256) are the eight set_border_horizontal "
          f"sprites drawn upward from y=$07")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
