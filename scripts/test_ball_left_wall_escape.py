#!/usr/bin/env python3
"""Regression test for ball reflection at the left wall near the bat.

Seeds the primary ball just inside the left wall (x=$09, dir=$2C up-left,
speed 2 — see the Makefile target) and asserts the 12-frame outcome of the
left-wall bounce.

Correct physics (bounce_wall $AC75 -> change_direction with mask $1F):
a side wall negates dx and PRESERVES dy, so the up-left $2C reflects to
($2C ^ $1F) + 1 = $34 — up-RIGHT, quadrant $30. The flight is fully
deterministic (no RNG / counter_misc dependence): bounce on frame 1, then
dx=+97/256*2 and dy=-230/256*2 per frame put the ball at exactly
x=$0F, y=$8A after the 12 probed frames.

HISTORY: this test originally asserted quadrant $00/$10 and x>$10 — values
tuned to the buggy 0x3F-dir reflect, which flipped dy at a side wall
(vertical flip). Under that bug the ball came off the wall moving DOWN
into the bat seeded below, and the BAT's deflection shot it away — the
test read that as a wall "escape" and enshrined the wrong physics. After
the reflect fix (notes/wall-bounce.md) the genuine wall bounce leaves the
wall up-right at a steep angle, and the old thresholds mis-fired.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu


FLOPPY = Path("build/batty-test.img")
OUT = Path("build/test_ball_left_wall_escape")


def read_probe_from_floppy() -> dict[str, str]:
    raw = subprocess.check_output(
        ["mtype", "-i", str(FLOPPY), "::PROBE.TXT"],
        stderr=subprocess.STDOUT,
    ).decode("ascii", errors="replace")
    values: dict[str, str] = {}
    for line in raw.splitlines():
        if line.startswith("#"):
            continue
        if ":" in line:
            k, v = line.split(":", 1)
        elif "=" in line:
            k, v = line.split("=", 1)
        else:
            continue
        values[k.strip()] = v.strip()
    return values


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["mdel", "-i", str(FLOPPY), "::PROBE.TXT"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    script = [
        "SLEEP 9.0",
        "sendkey ret",
        "SLEEP 1.5",
    ]
    run_qemu(FLOPPY, script, OUT / "qemu.log")

    probe = read_probe_from_floppy()
    ball = probe.get("object_ball_1", "")
    frame_probe = probe.get("frame_probe_state", "")
    if len(ball) != 44:
        raise SystemExit(f"FAIL: malformed object_ball_1 probe: {ball!r}")
    if len(frame_probe) != 10:
        raise SystemExit(f"FAIL: malformed frame_probe_state probe: {frame_probe!r}")

    x = int(ball[4:6], 16)
    y = int(ball[8:10], 16)
    direction = int(ball[12:14], 16)
    speed = int(ball[14:16], 16)
    frames = int(frame_probe[0:4], 16)
    remaining = int(frame_probe[4:8], 16)
    active = int(frame_probe[8:10], 16)

    if frames != 12 or remaining != 0 or active != 1:
        raise SystemExit(f"FAIL: frame probe did not stop after 12 frames: {frame_probe!r}")
    if speed != 0x02:
        raise SystemExit(f"FAIL: ball speed corrupted: speed=${speed:02X}")
    if direction != 0x34:
        raise SystemExit(
            f"FAIL: left-wall reflect must negate dx and preserve dy "
            f"($2C -> $34): dir=${direction:02X}, x=${x:02X}, y=${y:02X}")
    if (x, y) != (0x0F, 0x8A):
        raise SystemExit(
            f"FAIL: 12-frame post-bounce position drifted (expect x=$0F "
            f"y=$8A): x=${x:02X}, y=${y:02X}, dir=${direction:02X}")

    print(f"PASS ball_left_wall_escape: x=${x:02X}, y=${y:02X}, dir=${direction:02X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
