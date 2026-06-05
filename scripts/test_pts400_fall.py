#!/usr/bin/env python3
"""+400 score-popup motion gate (port-only, RNG-independent).

The +400 catch popup moves under `motion_accel_step(&pts_400_motion,
0x0028, 0x80)` in step_pts_400 — a DIFFERENT accel constant pair than the
bonus/bomb fall (0x0008/0x02), so this guards a faster-grow accumulator
path those gates don't reach (de=0x28 is 5x the bonus rate; cap_hi=0x80).
Code-comparison-verified vs the original LA590 marker motion, previously
ungated.

Bakes a fresh popup (BATTY_REPLAY_PTS400=x,y, dx zeroed so the y is pure)
with the ball hidden + no-ball-death suppressed, then probes
`pts400_state` at f10/f20/f30 and asserts pts_400_y matches the
independently hand-computed progression from y0=40:
  f10 -> 48, f20 -> 72, f30 -> 112  (still active, y < PLAYFIELD_H=192).

+-2 px slack for the QEMU-boot / WAIT_KEY-release frame jitter. The faster
accel (vs bonus) makes a constant regression even more obvious.

ZEsarUX-free (port-only); needs QEMU + mtools. See scripts/test_bonus_fall.py
/ test_bomb_fall.py and src/main.c step_pts_400 / motion_accel_step.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
PTS400_Y0 = 40
# (probe frame, expected pts_400_y) — hand-computed accel from y0=40, de=0x28.
CASES = [(10, 48), (20, 72), (30, 112)]
TOL = 2


def probe_pts400(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_pts400.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_PTS400=8,{PTS400_Y0} "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_pts400"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(
        r"pts400_state=active([0-9A-Fa-f]{2})_x[0-9A-Fa-f]{2}_y([0-9A-Fa-f]{2})",
        probe.read_text())
    if not m:
        return None
    return int(m.group(1), 16), int(m.group(2), 16)


def main() -> int:
    ok = True
    for frame, ey in CASES:
        r = probe_pts400(frame)
        if r is None:
            print(f"  frame {frame}: NO pts400_state in PROBE.TXT [FAIL]")
            ok = False
            continue
        active, y = r
        good = (active == 1 and abs(y - ey) <= TOL)
        ok = ok and good
        print(f"  frame {frame}: active={active} y={y} "
              f"[{'PASS' if good else 'FAIL'}] (expect active=1 y={ey}+-{TOL})")
    if ok:
        print("PASS pts400_fall: +400 popup matches the accel progression "
              "(y 48/72/112 at f10/20/30) — step_pts_400 0x28/0x80 guarded")
        return 0
    print("FAIL pts400_fall")
    return 1


if __name__ == "__main__":
    sys.exit(main())
