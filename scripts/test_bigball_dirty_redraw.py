#!/usr/bin/env python3
"""Compare big-ball (SMASH) dirty redraw against a full redraw baseline.

big-ball is the primary ball drawn with SPR_BIG_BALL — same 16×12 footprint
as the normal ball, so the existing primary dirty mark covers it and it
needs no full-dynamic blocker. Verifies that removing the big-ball blocker
keeps the render pixel-exact: activates big-ball via the deterministic
BATTY_REPLAY_BIGBALL hook (sets big_ball_ticks + bat.bonus_applied=0x07),
bakes the primary low so a short probe stays clear of bricks, probes f3 and
asserts the dirty path equals the BATTY_FORCE_BALL_FULL_REDRAW baseline.
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
OUT = Path("build/test_bigball_dirty_redraw")
BALL_OBJECT = "02008000A0001802020C000008070000000000000080"
PLAYFIELD_ROI = (0, 0, 256, 192)


def build_floppy(force_full: bool) -> None:
    env = os.environ.copy()
    env.update({
        "BATTY_START_LEVEL": "1",
        "BATTY_REPLAY_WAIT_KEY": "1",
        "BATTY_REPLAY_BALL_OBJECT": BALL_OBJECT,
        "BATTY_REPLAY_BALL_STUCK": "0",
        "BATTY_REPLAY_BIGBALL": "1",
        "BATTY_SUPPRESS_NO_BALL_DEATH": "1",
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
    ppm = out / "bigball_after_frames.ppm"
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
        make_diff_png(dirty, full, OUT / "bigball_dirty_redraw_diff.png")
        raise SystemExit(
            f"FAIL: big-ball dirty redraw differs from full redraw: "
            f"{diff} px [roi {PLAYFIELD_ROI}]"
        )
    print(f"PASS bigball_dirty_redraw: dirty redraw matches full redraw [roi {PLAYFIELD_ROI}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
