#!/usr/bin/env python3
"""Capture the original's enemy (bird/UFO) flight from the L3 state.

Unlike the multi-ball secondaries (which need the full triple-ball spawn
sequence to become live), the `l3-brick-flash` snapshot ALREADY has a live,
coherently-moving enemy at object_enemy ($9B96). This frame-steps the
original and probes that object so the enemy's exact trajectory + heading
can be compared against the port's handling_bird steering.

See notes/enemy-movement.md for the LAA7D decode this validates against.

Usage:
  python3 scripts/capture_enemy_flight.py            # frames 0..24
  python3 scripts/capture_enemy_flight.py --frames 60 --step 5
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENEMY_ADDR = 0x9B96


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--frames", type=int, default=24, help="last frame")
    ap.add_argument("--step", type=int, default=1)
    ap.add_argument("--replay", default="replays/l3-brick-flash.json")
    args = ap.parse_args()

    frame_list = ",".join(str(i) for i in range(0, args.frames + 1, args.step))
    p = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "capture_frame_timeline_original.py"),
         "--setup-from-replay", args.replay, "--frame-pc", "0x0038",
         "--probe-ball", hex(ENEMY_ADDR), "--frames", frame_list],
        capture_output=True, text=True, timeout=300)
    for line in p.stdout.splitlines():
        if "ball@frame" in line:
            print(line.replace("ball@", "enemy@"))
    return 0 if "PASS" in p.stdout else 1


if __name__ == "__main__":
    sys.exit(main())
