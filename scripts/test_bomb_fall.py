#!/usr/bin/env python3
"""Enemy-bomb fall motion gate (port-only, RNG-independent).

The dropped enemy bomb falls under the SAME accel family as a bonus —
`motion_accel_step(&bomb_motion, 0x0008, 0x02)` in step_bomb — so its
descent rate (which sets how dodgeable a bomb is) follows the identical
progression. Code-comparison-verified vs the original bomb_appear / fall,
but had no standing gate; this guards the bomb's own 0x08/0x02 call site
(a regression here would change bomb difficulty without touching the bonus
gate).

Bakes a fresh falling bomb (BATTY_REPLAY_BOMB=x,y) clear of the bat (x=8,
so no bat-kill) with the ball hidden + no-ball-death suppressed, then
probes `bomb_state` at f20/f40/f60 and asserts bomb_y matches the
independently hand-computed accel progression from y0=40:
  f20 -> 46, f40 -> 65, f60 -> 97  (still active, not hit/off-screen).

+-2 px slack for the QEMU-boot / WAIT_KEY-release frame jitter. A real
constant regression (e.g. de=0x10) roughly doubles the fall — far outside.

ZEsarUX-free (port-only); needs QEMU + mtools. See scripts/test_bonus_fall.py
(the bonus uses the same accel) and src/main.cpp step_bomb / motion_accel_step.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BOMB_Y0 = 40
# (probe frame, expected bomb_y) — hand-computed accel fall from y0=40.
CASES = [(20, 46), (40, 65), (60, 97)]
TOL = 2


def probe_bomb(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_bomb.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_BOMB=8,{BOMB_Y0} "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_bomb"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(
        r"bomb_state=active([0-9A-Fa-f]{2})_x[0-9A-Fa-f]{2}_y([0-9A-Fa-f]{2})",
        probe.read_text())
    if not m:
        return None
    return int(m.group(1), 16), int(m.group(2), 16)


def main() -> int:
    ok = True
    for frame, ey in CASES:
        r = probe_bomb(frame)
        if r is None:
            print(f"  frame {frame}: NO bomb_state in PROBE.TXT [FAIL]")
            ok = False
            continue
        active, y = r
        good = (active == 1 and abs(y - ey) <= TOL)
        ok = ok and good
        print(f"  frame {frame}: active={active} y={y} "
              f"[{'PASS' if good else 'FAIL'}] (expect active=1 y={ey}+-{TOL})")
    if ok:
        print("PASS bomb_fall: enemy bomb matches the accel progression "
              "(y 46/65/97 at f20/40/60) — step_bomb motion_accel_step guarded")
        return 0
    print("FAIL bomb_fall")
    return 1


if __name__ == "__main__":
    sys.exit(main())
