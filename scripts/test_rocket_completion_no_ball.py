#!/usr/bin/env python3
"""Compare rocket-clear hold frame against a forced full-flush baseline."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import PALETTE_RGB, make_diff_png, ppm_inner_to_indices, run_qemu, test_floppy


TEST_FLOPPY = Path(test_floppy())
OUT = Path("build/test_rocket_completion_no_ball")
BALL_AREA_ROI = (80, 140, 180, 188)


def source_guard() -> None:
    src = Path("src/main.cpp").read_text()
    required = [
        "BALL_HIDE();\n    ball.stuck = 0;\n    ball.extra2_active = 0;",
        "force_full_flush = 1;\n                redraw_full_with_ball(i);",
        "BATTY_HOLD_ROCKET_CLEAR",
    ]
    missing = [needle for needle in required if needle not in src]
    if missing:
        raise SystemExit("FAIL: rocket clear path no longer hides balls and redraws before the pause")


def build_floppy(force_full: bool) -> None:
    env = os.environ.copy()
    env.update(
        {
            "BATTY_START_LEVEL": "1",
            "BATTY_REPLAY_WAIT_KEY": "1",
            "BATTY_REPLAY_ROCKET_ACTIVE": "1",
            "BATTY_HOLD_ROCKET_CLEAR": "1",
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
    ppm = out / "rocket_clear.ppm"
    script = [
        "SLEEP 9.0",
        "sendkey ret",
        "SLEEP 8.0",
        f"screendump {ppm}",
        "sendkey ret",
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
    source_guard()

    build_floppy(force_full=False)
    dirty = capture("dirty")
    build_floppy(force_full=True)
    full = capture("full")

    diff = roi_diff(dirty, full, BALL_AREA_ROI)
    if diff != 0:
        make_diff_png(dirty, full, OUT / "rocket_completion_no_ball_diff.png")
        raise SystemExit(
            f"FAIL: rocket-clear ball area differs from full flush: {diff} px [roi {BALL_AREA_ROI}]"
        )
    print("PASS rocket_completion_no_ball: rocket clear hold frame has no stale ball/bat pixels")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
