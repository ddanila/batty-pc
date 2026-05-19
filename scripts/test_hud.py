#!/usr/bin/env python3
"""Focused in-game HUD regression test.

The main visual test uses a scoreless executable so the level GTs remain
stable. This test boots the normal build, enters L1, and compares the
stable in-game HUD pieces against an original ZEsarUX capture:

  - 1UP / HI / 2UP label row
  - player-1 zero score
  - player-2 zero score

The HI score digits are deliberately excluded because HISCORE.DAT can
change between local runs.
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import (  # noqa: E402
    PALETTE_RGB,
    expected_from_scr,
    make_diff_png,
    ppm_inner_to_indices,
    run_qemu,
)


ROIS = [
    ("hud_labels", (24, 12, 232, 20)),
    ("hud_p1_score", (16, 21, 64, 29)),
    ("hud_p2_score", (192, 21, 240, 29)),
]


def diff_roi(actual, expected, roi):
    x0, y0, x1, y1 = roi
    diff = 0
    total = (x1 - x0) * (y1 - y0)
    for y in range(y0, y1):
        row = y * 256
        for x in range(x0, x1):
            i = row + x
            if PALETTE_RGB[actual[i]] != PALETTE_RGB[expected[i]]:
                diff += 1
    return diff, total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--floppy", default="build/batty.img")
    ap.add_argument("--out", default="build/test_hud")
    ap.add_argument("--expected", default="build/snapshots/20260513T202101Z/screen.scr")
    ap.add_argument("--boot-wait", type=float, default=10.0)
    ap.add_argument("--state-wait", type=float, default=1.5)
    args = ap.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    ppm = out / "l1_start.ppm"

    script = [
        f"SLEEP {args.boot_wait}",
        "sendkey ret",
        f"SLEEP {args.state_wait}",
        "sendkey ret",
        f"SLEEP {args.state_wait}",
        "sendkey ret",
        f"SLEEP {args.state_wait}",
        f"screendump {ppm}",
        "SLEEP 0.3",
        "sendkey esc",
    ]

    print(f"booting {args.floppy} and capturing L1 HUD...")
    run_qemu(Path(args.floppy), script, out / "qemu.log")

    actual = ppm_inner_to_indices(ppm)
    expected = expected_from_scr(Path(args.expected))

    failed = 0
    for label, roi in ROIS:
        diff, total = diff_roi(actual, expected, roi)
        if diff == 0:
            print(f"  PASS {label}: pixel-identical ({total} px) [roi {roi}]")
        else:
            make_diff_png(actual, expected, out / f"{label}_diff.png")
            print(f"  FAIL {label}: {diff}/{total} px differ [roi {roi}]")
            print(f"        diff -> {out}/{label}_diff.png")
            failed += 1

    return failed


if __name__ == "__main__":
    raise SystemExit(main())
