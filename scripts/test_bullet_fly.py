#!/usr/bin/env python3
"""Laser-bullet flight motion gate (port-only, RNG-independent).

The laser bullet rises at a constant BULLET_SPEED (6 px/frame) in
step_bullet_one — `bullet_y -= 6`, deactivating at y<0 (fly-off) or on a
brick/alien hit (blast). This is the laser's only currently-gateable piece
(the fire CADENCE needs held-fire input the harness can't drive); it guards
the bullet travel speed, previously ungated.

Bakes an in-flight bullet (BATTY_REPLAY_BULLET=x,y) into slot 0, low in the
playfield (y0=170) and probes `bullet_state` at f2/f4/f6 — a window that
stays BELOW the brick field (y>128) so the bullet travels without blasting.
Asserts bullet_y matches the linear rise from y0=170:
  f2 -> 158, f4 -> 146, f6 -> 134  (still active).

NOTE the tolerance: unlike the accelerating falling-object gates (which
start at ~0 px/frame and so are insensitive to a +-1-frame boot offset),
the bullet moves a full 6 px from frame 1, so a 1-frame WAIT_KEY-release
jitter would shift y by 6. The baked + hidden-ball scenario has proven
frame-deterministic for the other motion gates, so TOL is kept tight; if a
boot-jitter false-fail ever appears, widen to 6 (one frame).

ZEsarUX-free (port-only); needs QEMU + mtools. See scripts/test_bonus_fall.py
and src/main.c step_bullet_one.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BULLET_Y0 = 170
# (probe frame, expected bullet_y) — linear rise 6 px/frame from y0=170.
CASES = [(2, 158), (4, 146), (6, 134)]
TOL = 2


def probe_bullet(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_bullet.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_BULLET=120,{BULLET_Y0} "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_bullet"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(
        r"bullet_state=active([0-9A-Fa-f]{2})_x[0-9A-Fa-f]{2}_y([0-9A-Fa-f]{2})",
        probe.read_text())
    if not m:
        return None
    return int(m.group(1), 16), int(m.group(2), 16)


def main() -> int:
    ok = True
    for frame, ey in CASES:
        r = probe_bullet(frame)
        if r is None:
            print(f"  frame {frame}: NO bullet_state in PROBE.TXT [FAIL]")
            ok = False
            continue
        active, y = r
        good = (active == 1 and abs(y - ey) <= TOL)
        ok = ok and good
        print(f"  frame {frame}: active={active} y={y} "
              f"[{'PASS' if good else 'FAIL'}] (expect active=1 y={ey}+-{TOL})")
    if ok:
        print("PASS bullet_fly: laser bullet rises at 6 px/frame "
              "(y 158/146/134 at f2/4/6) — BULLET_SPEED / step_bullet_one guarded")
        return 0
    print("FAIL bullet_fly")
    return 1


if __name__ == "__main__":
    sys.exit(main())
