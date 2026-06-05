#!/usr/bin/env python3
"""Compare bat fire-animation dirty redraw against a forced full baseline.

Verifies redraw_bat_dirty: while the laser cannon fire-animation plays, the
dirty tiers now refresh + flush the whole 13px bat body (the cannon-flash
sprite changes each frame) instead of forcing a full recompose. Auto-fires
the laser and probes f3 (fired f1, bat_fire_anim_ticks still active), then
asserts the dirty-path screen equals the BATTY_FORCE_BALL_FULL_REDRAW
baseline (no stale fire-sprite pixels left behind).
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import PALETTE_RGB, make_diff_png, ppm_inner_to_indices, run_qemu


TEST_FLOPPY = Path("build/batty-test.img")
OUT = Path("build/test_bat_fire_dirty_redraw")
BAT_LASER = "01017400AD000000040DEFAE1C0A74AD040DF0000180"
BALL_OBJECT = "02008000A0001802020C000008070000000000000080"
PLAYFIELD_ROI = (0, 0, 256, 192)


def build_floppy(force_full: bool) -> None:
    env = os.environ.copy()
    env.update({
        "BATTY_START_LEVEL": "1",
        "BATTY_REPLAY_WAIT_KEY": "1",
        "BATTY_REPLAY_BAT_OBJECT": BAT_LASER,
        "BATTY_AUTO_FIRE": "1",
        "BATTY_SUPPRESS_NO_BALL_DEATH": "1",
        "BATTY_REPLAY_BALL_OBJECT": BALL_OBJECT,
        "BATTY_REPLAY_BALL_STUCK": "0",
        "BATTY_VISUAL_PROBE_FRAMES": "3",
    })
    if force_full:
        env["BATTY_FORCE_BALL_FULL_REDRAW"] = "1"
    else:
        env.pop("BATTY_FORCE_BALL_FULL_REDRAW", None)
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env)


def capture(label: str) -> bytes:
    out = OUT / label
    out.mkdir(parents=True, exist_ok=True)
    ppm = out / "bat_fire_after_frames.ppm"
    script = [
        "SLEEP 9.0",
        "sendkey ret",
        "SLEEP 1.2",
        f"screendump {ppm}",
        "sendkey esc",
        "SLEEP 0.2",
    ]
    run_qemu(TEST_FLOPPY, script, out / "qemu.log")
    return ppm_inner_to_indices(ppm)


def roi_diff(actual: bytes, expected: bytes, roi: tuple[int, int, int, int]) -> int:
    x0, y0, x1, y1 = roi
    diff = 0
    for y in range(y0, y1):
        row = y * 256
        for x in range(x0, x1):
            i = row + x
            if PALETTE_RGB[actual[i]] != PALETTE_RGB[expected[i]]:
                diff += 1
    return diff


def main() -> int:
    shutil.rmtree(OUT, ignore_errors=True)
    OUT.mkdir(parents=True, exist_ok=True)

    build_floppy(force_full=False)
    dirty = capture("dirty")
    build_floppy(force_full=True)
    full = capture("full")

    diff = roi_diff(dirty, full, PLAYFIELD_ROI)
    if diff != 0:
        make_diff_png(dirty, full, OUT / "bat_fire_dirty_redraw_diff.png")
        raise SystemExit(
            f"FAIL: bat fire-anim dirty redraw differs from full redraw: "
            f"{diff} px [roi {PLAYFIELD_ROI}]"
        )
    print(f"PASS bat_fire_dirty_redraw: dirty redraw matches full redraw [roi {PLAYFIELD_ROI}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
