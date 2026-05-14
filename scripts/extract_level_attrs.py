#!/usr/bin/env python3
"""Extract the FULL 24-char-row attribute band from each GT screen.

Per level: 24 rows x 32 cols = 768 bytes (the entire 32x24-cell ZX
attribute area). 15 levels x 768 B = 11520 B total.

Char-row offsets:
   0..1   - top HUD (1UP/HI/2UP titles + scores)
   2..13  - brick field
  14..21  - empty playfield + side-frame attrs
  22..23  - bottom (bat / lives indicators)
"""
import sys
from pathlib import Path

SCR_DIR  = Path('build/level_gt')
N_LEVELS = 15
ATTR_BASE   = 6144
ROWS_PER_LV = 24
COLS        = 32


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/level_attrs.bin')
    buf = bytearray()
    for n in range(1, N_LEVELS + 1):
        scr = SCR_DIR / f'level_{n:02d}.scr'
        if not scr.exists():
            sys.exit(f'missing: {scr} — run scripts/capture_levels.py first')
        data = scr.read_bytes()
        buf.extend(data[ATTR_BASE : ATTR_BASE + ROWS_PER_LV * COLS])
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(buf))
    print(f'wrote {out_path} ({len(buf)} B for {N_LEVELS} levels)')


if __name__ == '__main__':
    main()
