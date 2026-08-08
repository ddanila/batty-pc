#!/usr/bin/env python3
"""Compare narrow bat redraw against a forced full redraw baseline."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import PALETTE_RGB, make_diff_png, ppm_inner_to_indices, run_qemu, test_floppy


TEST_FLOPPY = Path(test_floppy())
OUT = Path("build/test_bat_redraw_window")
BAT_RIGHT_KEYS = 6
BAT_ROI = (64, 160, 192, 192)


def build_floppy(force_full: bool) -> None:
    env = os.environ.copy()
    env.update(
        {
            "BATTY_START_LEVEL": "1",
            "BATTY_REPLAY_WAIT_KEY": "1",
            "BATTY_HIDE_BALL": "1",
            "BATTY_SUPPRESS_NO_BALL_DEATH": "1",
        }
    )
    if force_full:
        env["BATTY_FORCE_BAT_FULL_REDRAW"] = "1"
    else:
        env.pop("BATTY_FORCE_BAT_FULL_REDRAW", None)
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env)


def capture(label: str) -> bytes:
    out = OUT / label
    out.mkdir(parents=True, exist_ok=True)
    ppm = out / "bat_after_move.ppm"
    script = [
        "SLEEP 9.0",
        "sendkey ret",
        "SLEEP 0.5",
    ]
    for _ in range(BAT_RIGHT_KEYS):
        script.extend(["sendkey right", "SLEEP 0.25"])
    script.extend([f"screendump {ppm}", "SLEEP 0.2"])
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
    narrow = capture("narrow")
    build_floppy(force_full=True)
    full = capture("full")

    diff = roi_diff(narrow, full, BAT_ROI)
    if diff != 0:
        make_diff_png(narrow, full, OUT / "bat_redraw_window_diff.png")
        raise SystemExit(
            f"FAIL: narrow bat redraw differs from full redraw in bat band: "
            f"{diff} px [roi {BAT_ROI}]"
        )
    print(f"PASS bat_redraw_window: narrow redraw matches full redraw [roi {BAT_ROI}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
