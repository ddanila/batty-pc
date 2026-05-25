#!/usr/bin/env python3
"""Regression test for ball reflection at the left wall near the bat.

Seeds the primary ball just inside the left wall with an up-left
descriptor. The old XOR-mask reflection kept the descriptor moving left,
so the ball jittered along x=$08 instead of escaping back into the field.
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
    if x <= 0x10:
        raise SystemExit(f"FAIL: ball did not escape the left wall: x=${x:02X}, y=${y:02X}, dir=${direction:02X}")
    if direction & 0x30 not in (0x00, 0x10):
        raise SystemExit(f"FAIL: ball still points left after wall escape: dir=${direction:02X}")

    print(f"PASS ball_left_wall_escape: x=${x:02X}, y=${y:02X}, dir=${direction:02X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
