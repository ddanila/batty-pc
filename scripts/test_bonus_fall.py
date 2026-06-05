#!/usr/bin/env python3
"""Falling-bonus motion gate (port-only, RNG-independent).

A caught-or-missed bonus falls under `motion_accel_step(&bonus_motion,
0x0008, 0x02)` — a velocity accumulator that grows by 0x08/frame, capped at
0x0200, with an 8.8 fractional carry into the pixel step. That accelerating
fall was code-comparison-verified vs the original `handling_bonus`
(LA55A_0) but had NO standing gate; this guards the 0x08/0x02 constants and
the accumulator math against regression.

Bakes a fresh falling bonus (BATTY_REPLAY_BONUS=type,x,y) clear of the bat
(x=8) with the ball hidden + no-ball-death suppressed, so the only moving
thing is the bonus. Probes `bonus_state` at f20/f40/f60 and asserts the y
matches the independently hand-computed accel progression from y0=40:
  f20 -> 46, f40 -> 65, f60 -> 97  (still active, not caught/off-screen).

The +-2 px slack absorbs the QEMU-boot / WAIT_KEY-release frame jitter (the
same +-1-frame wobble the enemy gates see; at f60 the fall is ~2 px/frame).
A real constant/algorithm regression (e.g. de=0x10) roughly doubles the
fall (~+57 px at f60) — far outside the slack.

ZEsarUX-free (port-only); needs QEMU + mtools. See notes/parity-status.md
(bonus economy) and src/main.c step_bonus / motion_accel_step.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BONUS_Y0 = 40
# (probe frame, expected bonus_y) — hand-computed accel fall from y0=40.
CASES = [(20, 46), (40, 65), (60, 97)]
TOL = 2


def probe_bonus(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_bonus.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_BONUS=1,8,{BONUS_Y0} "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_bonus"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(
        r"bonus_state=active([0-9A-Fa-f]{2})_type[0-9A-Fa-f]{2}"
        r"_x[0-9A-Fa-f]{2}_y([0-9A-Fa-f]{2})_bomb([0-9A-Fa-f]{2})",
        probe.read_text())
    if not m:
        return None
    return int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16)


def main() -> int:
    ok = True
    for frame, ey in CASES:
        r = probe_bonus(frame)
        if r is None:
            print(f"  frame {frame}: NO bonus_state in PROBE.TXT [FAIL]")
            ok = False
            continue
        active, y, bomb = r
        good = (active == 1 and bomb == 0 and abs(y - ey) <= TOL)
        ok = ok and good
        print(f"  frame {frame}: active={active} y={y} bomb={bomb} "
              f"[{'PASS' if good else 'FAIL'}] (expect active=1 y={ey}+-{TOL} bomb=0)")
    if ok:
        print("PASS bonus_fall: falling bonus matches the accel progression "
              "(y 46/65/97 at f20/40/60) — motion_accel_step(0x08,0x02) guarded")
        return 0
    print("FAIL bonus_fall")
    return 1


if __name__ == "__main__":
    sys.exit(main())
