#!/usr/bin/env python3
"""The playfield's hex tile is tape data, and so is bg_attr_per_cycle.

`assets/bg_tile.bin` is built from the tape's four level textures:

    spr_level_texture_1   $C015
    spr_level_texture_2   $8EE8
    spr_level_texture_3   $8F10
    spr_level_texture_4   $8F38

each a standard sprite block — `(w=2, h=$10)`, 32 pattern bytes, then
`(2, 2)` and four attribute bytes.

Two things are checked, and the second is the one a build cannot notice:

  PATTERN  the blob equals the textures with their rows REVERSED.
           `print_sprite_pix` stacks upward and the texture loop starts
           at `LD HL,$0F00` — y=15 — so within each 16-row band the
           texture's row 0 sits at the bottom, while `paint_bg_to_buff`
           indexes `ty = y & 15` from the top.

  ATTRS    each texture's attribute byte is that cycle's background
           attr, and `bg_attr_per_cycle[]` in src/main.cpp is a
           hand-written copy of those four bytes. Nothing else ties the
           two together; a drift would repaint every playfield in the
           wrong colour and no source gate would notice.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BLOCK = ROOT / "original/blocks/03_DATA_headless.dat.bin"
TILE = ROOT / "assets/bg_tile.bin"
SRC = ROOT / "src/main.cpp"

BASE = 0x6800
TEXTURES = (0xC015, 0x8EE8, 0x8F10, 0x8F38)
W, H = 2, 16


def main() -> int:
    data = BLOCK.read_bytes()
    tile = TILE.read_bytes()
    if len(tile) != len(TEXTURES) * W * H:
        raise SystemExit(f"FAIL: bg_tile.bin is {len(tile)} bytes, expected "
                         f"{len(TEXTURES) * W * H}")

    m = re.search(r"bg_attr_per_cycle\[4\]\s*=\s*\{(.*?)\};", SRC.read_text(),
                  re.S)
    if not m:
        raise SystemExit("FAIL: bg_attr_per_cycle[4] is not in src/main.cpp")
    port_attrs = [int(h, 16)
                  for h in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1))]
    if len(port_attrs) != 4:
        raise SystemExit(f"FAIL: bg_attr_per_cycle parsed to "
                         f"{len(port_attrs)} entries, not 4")

    checked = 0
    for cycle, addr in enumerate(TEXTURES):
        off = addr - BASE
        w, h = data[off], data[off + 1]
        if (w, h) != (W, H):
            raise SystemExit(f"FAIL: texture {cycle + 1} at ${addr:04X} has "
                             f"header ({w}, {h}), expected ({W}, {H}) — the "
                             f"block moved")
        px = data[off + 2:off + 2 + w * h]
        for j in range(h):
            src = (h - 1 - j) * w
            for k in range(w):
                got = tile[cycle * w * h + j * w + k]
                checked += 1
                if got != px[src + k]:
                    raise SystemExit(
                        f"FAIL: bg_tile.bin cycle {cycle} row {j} byte {k} is "
                        f"{got:02X}, the tape's reversed texture says "
                        f"{px[src + k]:02X}. Rows go UPWARD in the original; "
                        f"paint_bg_to_buff indexes from the top.")
        attr = data[off + 2 + w * h + 2]
        if attr != port_attrs[cycle]:
            raise SystemExit(
                f"FAIL: texture {cycle + 1} carries background attr "
                f"${attr:02X}, but bg_attr_per_cycle[{cycle}] is "
                f"${port_attrs[cycle]:02X}. That table is a hand-written "
                f"copy of these four bytes and nothing else ties them "
                f"together.")

    print(f"PASS bg_tile_derivable: {checked} tile bytes are the tape's four "
          f"level textures reversed, and bg_attr_per_cycle matches their "
          f"attribute bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
