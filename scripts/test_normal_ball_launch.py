#!/usr/bin/env python3
"""Regression test for the normal SPACE-launch ball path.

The L3 replay seeds object_ball_1 directly, so it does not exercise the
ordinary level-entry state where SPACE converts a stuck ball into an
in-flight primary ball. This test boots directly into L1, presses SPACE,
waits briefly, exits cleanly, and inspects PROBE.TXT from the floppy.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu


FLOPPY = Path("build/batty-test.img")
OUT = Path("build/test_normal_ball_launch")


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
    script = [
        "SLEEP 9.0",
        "sendkey ret",
        "SLEEP 0.3",
        "sendkey spc",
        "SLEEP 0.35",
        "sendkey esc",
        "SLEEP 0.5",
    ]
    run_qemu(FLOPPY, script, OUT / "qemu.log")

    probe = read_probe_from_floppy()
    ball = probe.get("object_ball_1", "")
    launch = probe.get("normal_launch_state", "")
    if len(ball) != 44:
        raise SystemExit(f"FAIL: malformed object_ball_1 probe: {ball!r}")
    if len(launch) != 10:
        raise SystemExit(f"FAIL: malformed normal_launch_state probe: {launch!r}")

    x = int(ball[4:6], 16)
    y = int(ball[8:10], 16)
    direction = int(ball[12:14], 16)
    speed = int(ball[14:16], 16)
    launch_valid = int(launch[0:2], 16)
    launch_x = int(launch[2:4], 16)
    launch_y = int(launch[4:6], 16)
    launch_dir = int(launch[6:8], 16)
    launch_speed = int(launch[8:10], 16)

    if launch_valid != 1:
        raise SystemExit("FAIL: normal SPACE launch did not record a launch")
    if (launch_x, launch_y) != (0x84, 0xA6):
        raise SystemExit(f"FAIL: normal launch started from unexpected position: x=${launch_x:02X} y=${launch_y:02X}")
    if launch_dir not in (0x1B, 0x24):
        raise SystemExit(f"FAIL: normal SPACE launch direction not initialized: dir=${launch_dir:02X}")
    if launch_speed != 0x02:
        raise SystemExit(f"FAIL: normal SPACE launch speed not initialized: speed=${launch_speed:02X}")
    if (x, y) == (0x84, 0xA6):
        raise SystemExit("FAIL: normal SPACE launch left the ball stuck on the bat")
    if y >= 0xA0:
        raise SystemExit(f"FAIL: normal SPACE launch did not move upward enough: y=${y:02X}")
    if speed != 0x02:
        raise SystemExit(f"FAIL: normal SPACE launch corrupted speed after release: speed=${speed:02X}")

    print(
        f"PASS normal_ball_launch: launch=x${launch_x:02X},y${launch_y:02X},dir${launch_dir:02X}; "
        f"probe=x${x:02X},y${y:02X},dir${direction:02X}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
