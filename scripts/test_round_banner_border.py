#!/usr/bin/env python3
"""Regression check for the original round-window top band."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import ppm_inner_to_indices, run_qemu, test_floppy


TEST_FLOPPY = Path(test_floppy())
OUT = Path("build/test_round_banner_border")
WINDOW_X, WINDOW_Y, WINDOW_W = 88, 133, 80
# The window is 32px tall (y=133..164); the "PLAYER 1" ink starts at
# y=138 in the original (the text is bottom-anchored and vertically
# centred, leaving a 5px black margin at the top). Only those 5 rows
# are guaranteed black -- an earlier value of 8 here encoded a bug where
# the port jammed the text 5px low against the box bottom.
TOP_BAND_H = 5


def source_guard() -> None:
    src = Path("src/main.cpp").read_text()
    if "int banner_y = BORDER_Y + 133;" not in src:
        raise SystemExit("FAIL: round banner must start at original window y=$85/$A458 -> playfield y=133")
    if "BATTY_HOLD_ROUND_BANNER" not in src:
        raise SystemExit("FAIL: missing stable round-banner hold hook for visual regression")


def build_floppy() -> None:
    env = os.environ.copy()
    env.update(
        {
            "BATTY_START_LEVEL": "1",
            "BATTY_HOLD_ROUND_BANNER": "1",
        }
    )
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env)


def capture() -> bytes:
    OUT.mkdir(parents=True, exist_ok=True)
    ppm = OUT / "round_banner.ppm"
    script = [
        "SLEEP 9.0",
        f"screendump {ppm}",
        "sendkey ret",
        "SLEEP 0.2",
    ]
    run_qemu(TEST_FLOPPY, script, OUT / "qemu.log")
    return ppm_inner_to_indices(ppm)


def main() -> int:
    shutil.rmtree(OUT, ignore_errors=True)
    source_guard()
    build_floppy()
    idx = capture()

    total = WINDOW_W * TOP_BAND_H
    black = 0
    for y in range(WINDOW_Y, WINDOW_Y + TOP_BAND_H):
        row = y * 256
        for x in range(WINDOW_X, WINDOW_X + WINDOW_W):
            if idx[row + x] in (0, 8):
                black += 1
    if black < total:
        raise SystemExit(
            f"FAIL: round banner top band is not fully black: {black}/{total} px"
        )
    print("PASS round_banner_border: top black band matches original window at x=88 y=133")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
