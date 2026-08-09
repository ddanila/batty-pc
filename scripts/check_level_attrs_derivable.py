#!/usr/bin/env python3
"""Every live brick's attribute byte in level_attrs.bin is DERIVED, not data.

`assets/level_attrs.bin` is 15 levels x 768 bytes of ZX attribute cells,
extracted from emulator captures (`scripts/extract_level_attrs.py`).
PLAN.md WS7 wants the captured blobs gone. This gate is the evidence for
the part that can go: it recomputes every LIVE BRICK's attribute pair
from tape-derived data and asserts the captured bytes agree, byte for
byte, over all 15 levels.

The derivation, and it is two rules:

    attr = briks_colors[cell & 0x0F]          both halves of the cell
    attr &= 0xBF  at char column 1            the border's drop shadow

`briks_colors` is `$AEEC`, a 16-byte table in the tape. The second rule
is `print_border_shadow` ($BFCF), called at the end of
`game_screen_draw_to_buffer`, AFTER `print_briks`:

    print_border_shadow:
      LD HL,attr_buff+$21 / LD B,$17 / LD DE,$0020
      LBFCF_0: RES 6,(HL) / ADD HL,DE / DJNZ LBFCF_0      ; col 1, rows 1..23
      LD HL,attr_buff+$22 / LD B,$1D
      LBFCF_1: RES 6,(HL) / INC L / DJNZ LBFCF_1          ; row 1, cols 2..30

Because it runs after the bricks, a brick in field column 0 has the
bright bit taken back off its LEFT char. That is the whole of the
discrepancy: without the shadow rule the match is 2374/2412, and every
one of the 38 misses differs by exactly $40.

### What this does and does not prove

It covers the 2412 live-brick bytes — 20.9% of the blob. The brick zone
(char rows 4..15, cols 1..30) is 5400 bytes, 46.9%; the rest of that
zone is empty cells, which carry side-strip and background colours this
gate does not yet derive. The other 53.1% is the HUD rows, the side
frame strips and the bottom bat/lives rows.

So: the blob cannot be deleted yet. What is settled is that a fifth of
it is redundant and that the rule reproducing it is exact rather than
approximate — which is the thing WS7 needs to know before porting the
rest of the writer.

It is also a regression gate on two things a screendump would only catch
indirectly: the `briks_colors` transcription, and a bad re-extraction of
level_attrs.bin (the extraction is manual and off the build graph —
see the note in the Makefile).
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ATTRS = ROOT / "assets/level_attrs.bin"
LEVELS = ROOT / "assets/levels.bin"
BRICKS_SRC = ROOT / "src/bricks.cpp"

N_LEVELS = 15
ROWS, COLS = 12, 15
BAND = 768                 # 24 char-rows x 32 cols
BRICK_CR0 = 4              # field row 0 lives at char row 4


def briks_colors() -> list:
    """Read the table out of src/bricks.cpp rather than restating it.

    A gate carrying its own copy of a table agrees with a wrong copy in
    the port as long as both are wrong the same way — the same reason
    check_kinnock parses txt_kinnock.asm instead of transcribing it.
    """
    text = BRICKS_SRC.read_text()
    m = re.search(r"briks_colors\[16\]\s*=\s*\{(.*?)\}", text, re.S)
    if not m:
        raise SystemExit("FAIL: could not find briks_colors[16] in "
                         "src/bricks.cpp — if the table moved, point this "
                         "gate at it")
    body = re.sub(r"/\*.*?\*/", " ", m.group(1), flags=re.S)
    vals = [int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{2})", body)]
    if len(vals) != 16:
        raise SystemExit(f"FAIL: briks_colors parsed to {len(vals)} entries, "
                         f"not 16")
    return vals


def main() -> int:
    if not ATTRS.exists() or not LEVELS.exists():
        raise SystemExit("FAIL: assets/level_attrs.bin or assets/levels.bin "
                         "is missing — run the asset extraction first")

    table = briks_colors()
    attrs = ATTRS.read_bytes()
    levels = LEVELS.read_bytes()
    if len(attrs) != N_LEVELS * BAND:
        raise SystemExit(f"FAIL: level_attrs.bin is {len(attrs)} bytes, "
                         f"expected {N_LEVELS * BAND}")

    checked = 0
    bad = []
    for lvl in range(N_LEVELS):
        band = attrs[lvl * BAND:(lvl + 1) * BAND]
        cells = levels[lvl * ROWS * COLS:(lvl + 1) * ROWS * COLS]
        for r in range(ROWS):
            for c in range(COLS):
                cell = cells[r * COLS + c]
                if (cell & 0x80) or (cell & 0x0F) == 0:
                    continue            # destroyed marker / empty sentinel
                base = table[cell & 0x0F]
                for cc in (1 + 2 * c, 2 + 2 * c):
                    # print_border_shadow's left arm, applied after the
                    # bricks, takes the bright bit off char column 1.
                    want = (base & 0xBF) if cc == 1 else base
                    got = band[(BRICK_CR0 + r) * 32 + cc]
                    checked += 1
                    if got != want:
                        bad.append((lvl + 1, r, c, cc, want, got))

    if bad:
        print(f"FAIL: {len(bad)} of {checked} live-brick attribute bytes do "
              f"not match the derivation.\n")
        for lvl, r, c, cc, want, got in bad[:20]:
            note = ""
            if got == (want ^ 0x40):
                note = ("  (differs only in bit 6 — the border shadow, "
                        "print_border_shadow $BFCF)")
            print(f"  level {lvl:2d} row {r:2d} col {c:2d} char-col {cc:2d}: "
                  f"want {want:02X} got {got:02X}{note}")
        if len(bad) > 20:
            print(f"  ... and {len(bad) - 20} more")
        print("\nEither briks_colors drifted from $AEEC, the border-shadow "
              "rule changed, or level_attrs.bin was re-extracted from a "
              "capture that does not match assets/levels.bin.")
        return 1

    pct = 100.0 * checked / len(attrs)
    print(f"PASS level_attrs_derivable: all {checked} live-brick attribute "
          f"bytes ({pct:.1f}% of level_attrs.bin) are reproduced by "
          f"briks_colors + print_border_shadow")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
