#!/usr/bin/env python3
"""Residue repro: enemy crossing the brick band while the ball destroys
bricks (known-bugs.md #1/#2). Runs the deterministic L3 brick-flash
scenario (pinned RNG + counter) for N frames on the dirty path and on the
BATTY_FORCE_BALL_FULL_REDRAW baseline; any leftover the dirty path fails
to erase shows up as a pixel diff against the full recompose.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import PALETTE_RGB, make_diff_png, ppm_inner_to_indices, run_qemu, test_floppy


TEST_FLOPPY = Path(test_floppy())
OUT = Path("build/test_enemy_brick_residue")
BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BALL_OBJECT = "02006C004E001F03020CEEF008076C4E020C0000008C"
ENEMY_OBJECT = "0905A4471B642D01030FDD74180CA41C030F30703100"
PLAYFIELD_ROI = (0, 0, 256, 192)
PROBE_FRAMES = "80"


def build_floppy(force_full: bool) -> None:
    env = os.environ.copy()
    env.update(
        {
            "BATTY_LEVEL": "3",
            "BATTY_START_LEVEL": "1",
            "BATTY_REPLAY_WAIT_KEY": "1",
            "BATTY_REPLAY_RANDOM": "3793",
            "BATTY_REPLAY_RANDOM_SEED": "962A",
            "BATTY_REPLAY_BAT_OBJECT": BAT_OBJECT,
            "BATTY_REPLAY_BALL_OBJECT": BALL_OBJECT,
            "BATTY_REPLAY_BALL_STUCK": "0",
            "BATTY_REPLAY_ENEMY_OBJECT": ENEMY_OBJECT,
            "BATTY_REPLAY_COUNTER": "0",
            "BATTY_SUPPRESS_NO_BALL_DEATH": "1",
            "BATTY_VISUAL_PROBE_FRAMES": PROBE_FRAMES,
        }
    )
    if force_full:
        env["BATTY_FORCE_FULL_FLUSH_EACH_FRAME"] = "1"
    else:
        env.pop("BATTY_FORCE_FULL_FLUSH_EACH_FRAME", None)
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env)


def capture(label: str) -> bytes:
    out = OUT / label
    out.mkdir(parents=True, exist_ok=True)
    ppm = out / "after_frames.ppm"
    script = [
        "SLEEP 9.0",
        "sendkey ret",
        "SLEEP 8.0",   # frame-80 probe halt; full-flush runs are slow, wait generously
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
        make_diff_png(dirty, full, OUT / "enemy_brick_residue_diff.png")
        raise SystemExit(
            f"FAIL: dirty path leaves residue vs full redraw after "
            f"{PROBE_FRAMES} frames: {diff} px [roi {PLAYFIELD_ROI}] "
            f"(diff png in {OUT})"
        )
    print(
        f"PASS enemy_brick_residue: dirty redraw matches full redraw after "
        f"{PROBE_FRAMES} frames [roi {PLAYFIELD_ROI}]"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
