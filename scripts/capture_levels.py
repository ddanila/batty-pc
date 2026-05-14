#!/usr/bin/env python3
"""Capture ground-truth PNGs of all 15 levels by patching the original
in ZEsarUX.

Usage: python3 scripts/capture_levels.py [--start N] [--count K]
       (N is 1-based; K defaults to N_LEVELS-N+1, i.e. through the end)

  - load snap3.sna  (any active gameplay state will do)
  - for N in 0..14:
      - patch (0xB7EA) := N  (level counter, 0-based)
      - patch PC := 0xBA4C   (level-init label inside sub_b8e6h)
      - resume; let the game reach its frame-sync HALT loop
      - save-screen -> build/level_gt/level_NN.png

We resume each level with `exit-cpu-step` + a fixed wall-clock
delay, then re-pause. ZEsarUX is fast enough that ~1.2 s real-time
gives plenty of frames for the level-init paint + sub_b765h to
finish.
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from zrcp import launch_emulator


ZESARUX  = Path(__file__).resolve().parent.parent / "../generaly/tools/zesarux/src/zesarux"
SNAP_DIR = Path("build/snapshots/20260513T202101Z")
SNA_PATH = SNAP_DIR.parent / f"{SNAP_DIR.name}.sna"
OUT_DIR  = Path("build/level_gt")

LEVEL_INIT_PC = 0xBA4C
LEVEL_COUNTER = 0xB7EA
N_LEVELS = 15
SETTLE_SECONDS = 1.2


def ensure_sna():
    if not SNA_PATH.exists() or SNA_PATH.stat().st_mtime < (SNAP_DIR / 'ram_4000_FFFF.bin').stat().st_mtime:
        subprocess.run(['python3', str(Path(__file__).parent / 'snap_to_sna.py'),
                        str(SNAP_DIR), str(SNA_PATH)], check=True)


def capture_one(zc, level_n):
    zc.enter_cpu_step()
    zc.snapshot_load(str(SNA_PATH.resolve()))
    zc.enter_cpu_step()                          # snapshot-load resumes the CPU
    zc.write_memory(LEVEL_COUNTER, bytes([level_n]))
    zc.set_register('PC', LEVEL_INIT_PC)
    zc.exit_cpu_step()
    time.sleep(SETTLE_SECONDS)
    zc.enter_cpu_step()
    scr = OUT_DIR / f"level_{level_n+1:02d}.scr"
    zc.save_screen(str(scr.resolve()))
    return scr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--start', type=int, default=1, help='first level (1-based)')
    ap.add_argument('--count', type=int, default=None, help='how many to capture')
    args = ap.parse_args()
    first = max(1, args.start)
    last  = min(N_LEVELS, first + (args.count or N_LEVELS - first + 1) - 1)

    ensure_sna()
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    print(f"launching ZEsarUX headless (capturing levels {first}..{last})...")
    proc, zc = launch_emulator(str(ZESARUX), machine='48k',
                               extra_args=[], port=10000, headless=True)
    try:
        print(f"  connected: {zc.get_version()}")
        scrs = []
        for n in range(first - 1, last):
            scr = capture_one(zc, n)
            ok = scr.exists() and scr.stat().st_size == 6912
            print(f"  level {n+1:2d}: {scr.name}  {'OK' if ok else 'FAIL'}")
            if ok: scrs.append(scr)
    finally:
        try: zc.exit_emulator()
        except Exception: pass
        proc.wait(timeout=5)

    # Decode every .scr present (not just this run's), then grid.
    print("\ndecoding .scr -> .png...")
    sys.path.insert(0, str(Path(__file__).parent))
    from extract_scr import decode
    from PIL import Image
    def zx_pal():
        out = []
        for level in (0xE0, 0xFF):
            for ink in range(8):
                r = level if (ink & 2) else 0
                g = level if (ink & 4) else 0
                b = level if (ink & 1) else 0
                out.append((r, g, b))
        return out
    pal = zx_pal()
    GUTTER = 4
    grid = Image.new('RGB', (3 * (256 + GUTTER) + GUTTER, 5 * (192 + GUTTER) + GUTTER), (24, 24, 32))
    for n in range(1, N_LEVELS + 1):
        scr = OUT_DIR / f"level_{n:02d}.scr"
        if not scr.exists():
            slot = Image.new('RGB', (256, 192), (50, 50, 50))
        else:
            idx = decode(scr.read_bytes())
            slot = Image.new('RGB', (256, 192))
            for y in range(192):
                for x in range(256):
                    slot.putpixel((x, y), pal[idx[y*256 + x]])
            slot.save(scr.with_suffix('.png'))
        i = n - 1
        gx = (i % 3) * (256 + GUTTER) + GUTTER
        gy = (i // 3) * (192 + GUTTER) + GUTTER
        grid.paste(slot, (gx, gy))
    grid.save(OUT_DIR / 'all_levels_gt.png')
    print(f"wrote {OUT_DIR}/all_levels_gt.png")


if __name__ == '__main__':
    main()
