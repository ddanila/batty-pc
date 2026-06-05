#!/usr/bin/env python3
"""Compare multi-ball dirty redraw against a forced full redraw baseline.

KNOWN FLAKY (2026-06-05) — NOT wired into parity-check-full. The MULTI_BALL
catch scenario is too emergent for a clean dirty-vs-full comparison: the 3
balls bounce through the brick band, destroy bricks, and drop further
RNG-gated bonuses, and the screendump capture is not frame-frozen for it, so
results vary run-to-run (seen 0 px and 75 px on identical builds). It also
exposed a separate +400-popup simple-tier rendering coupling at early frames
(f5: 138 px at the catch position). The extra-ball dirty tier this was
written to verify is therefore PARKED until a deterministic harness exists:
a direct ball2/ball3 bake hook (no bonus catch -> no popup, balls placed
below the brick band -> no emergent hits). See notes/performance.md.

Verifies the extra-ball tier: catching a MULTI_BALL bonus spawns ball2/ball3,
which are now rendered + dirty-marked on the ball-object tier instead of
forcing a full-dynamic recompose. Bakes a MULTI_BALL bonus (port type 9)
just above the bat so it is caught on f1, then probes f5 (three balls in
flight) and asserts the dirty-path screen equals the
BATTY_FORCE_BALL_FULL_REDRAW baseline (no extra-ball trail).
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
OUT = Path("build/test_multiball_dirty_redraw")
BALL_OBJECT = "02008000A0001802020C000008070000000000000080"
PLAYFIELD_ROI = (0, 0, 256, 192)


def build_floppy(force_full: bool) -> None:
    env = os.environ.copy()
    env.update({
        "BATTY_START_LEVEL": "1",
        "BATTY_REPLAY_WAIT_KEY": "1",
        "BATTY_REPLAY_BALL_OBJECT": BALL_OBJECT,
        "BATTY_REPLAY_BALL_STUCK": "0",
        "BATTY_REPLAY_BONUS": "9,118,167",   # MULTI_BALL caught at the bat
        "BATTY_SUPPRESS_NO_BALL_DEATH": "1",
        "BATTY_VISUAL_PROBE_FRAMES": "30",
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
    ppm = out / "multiball_after_frames.ppm"
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
        make_diff_png(dirty, full, OUT / "multiball_dirty_redraw_diff.png")
        raise SystemExit(
            f"FAIL: multi-ball dirty redraw differs from full redraw: "
            f"{diff} px [roi {PLAYFIELD_ROI}]"
        )
    print(f"PASS multiball_dirty_redraw: dirty redraw matches full redraw [roi {PLAYFIELD_ROI}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
