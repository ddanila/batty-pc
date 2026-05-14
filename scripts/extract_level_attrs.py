#!/usr/bin/env python3
"""Extract the 12-row brick-attribute band from each GT screen.

Per level: 12 rows x 32 cols = 384 bytes (char rows 2..13 cover the
brick field exactly). 15 levels x 384 B = 5760 B total.
Output: a single concatenated blob suitable as a runtime asset.
"""
import sys
from pathlib import Path

SCR_DIR  = Path('build/level_gt')
N_LEVELS = 15
ATTR_BASE   = 6144            # offset into a .scr where attrs start
ROWS_PER_LV = 12              # char rows 2..13
COLS        = 32


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/level_attrs.bin')
    buf = bytearray()
    for n in range(1, N_LEVELS + 1):
        scr = SCR_DIR / f'level_{n:02d}.scr'
        if not scr.exists():
            sys.exit(f'missing: {scr} — run scripts/capture_levels.py first')
        data = scr.read_bytes()
        # Char rows 2..13 = bytes ATTR_BASE + 2*32 .. ATTR_BASE + 14*32
        buf.extend(data[ATTR_BASE + 2 * COLS : ATTR_BASE + 14 * COLS])
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(buf))
    print(f'wrote {out_path} ({len(buf)} B for {N_LEVELS} levels)')


if __name__ == '__main__':
    main()
