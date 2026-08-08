#!/usr/bin/env python3
"""Compare narrow bat redraw against a forced full redraw baseline.

Parks the bat against the LEFT wall. Two things follow from that.

Determinism: the two captures come from separate boots and QEMU
`sendkey` is wall-clock, so counting keypresses left the bat up to 12 px
apart between runs and the gate compared a bat at one X against a bat at
another — 5 failures in 8 runs, the diff varying 143..232 px because it
tracked the offset rather than a defect. `bat_step_x` clamps at
BAT_MARGIN_LEFT, so holding the key past saturation pins the end
position. `sendkey <key> <hold_ms>` is what gets there; discrete presses
land on about half a frame of key-down each and 40 of them bought only
~22 of the 27 frames needed.

Coverage: a clamped bat is the only way the bat's repaint window reaches
byte 1 or byte 30, which is where the narrow path used to drop
inner_border_line_c's two blacked columns (known-bugs.md #11). The ROI
is the whole bat band so both columns are compared.
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
OUT = Path("build/test_bat_redraw_window")
# 116 (BAT_X_INIT) -> 8 (clamp) is 108 px = 27 frames of held key at
# 4 px/frame; one 800 ms hold is ~40 frames, two leaves no doubt.
BAT_LEFT_HOLDS = 2
BAT_LEFT_HOLD_MS = 800
BAT_ROI = (0, 160, 256, 192)


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
    for _ in range(BAT_LEFT_HOLDS):
        script.extend([f"sendkey left {BAT_LEFT_HOLD_MS}",
                       f"SLEEP {BAT_LEFT_HOLD_MS / 1000.0 + 0.3:.1f}"])
    script.extend(["SLEEP 0.5", f"screendump {ppm}", "SLEEP 0.2"])
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
