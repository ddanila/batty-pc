#!/usr/bin/env python3
"""Laser fire-cadence gate (port-only, RNG-independent).

Holding fire with the LASER bonus, the bat shoots on a fixed cadence: the
cooldown resets to 0x18 (24) on each shot and decrements 2/frame, and the
fire gate checks `cooldown == 0` BEFORE that end-of-frame decrement — so a
new shot lands every 12 frames (shot 1 at f1, shot 2 at f13). This 0x18
value was a deliberate parity fix (a plain 0x16 gave 11 frames, ~8% too
fast); it was code-comparison-verified but ungated because the fire path is
edge-triggered by a SPACE keypress the capture harness can't hold down.

This gate adds a BATTY_AUTO_FIRE test hook that calls the (now extracted)
try_fire_laser() every frame — simulating held SPACE deterministically —
and a shots-fired counter exposed via `laser_fire_state`. With the bat
baked into LASER mode (bonus_applied=0x01) and the ball hidden, it probes
the shot count at f1/f12/f13 and asserts the 12-frame period:
  f1 -> 1 shot, f12 -> still 1, f13 -> 2.
A regression to 0x16 fires shot 2 at f12 (so f12 -> 2), which this catches.

ZEsarUX-free (port-only); needs QEMU + mtools. See src/main.cpp try_fire_laser
and notes/parity-status.md.
"""
import re
import subprocess
import sys
from pathlib import Path

from test_visual import test_floppy

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = test_floppy()

# Steer-gate bat with byte 0x14 (bonus_applied) set to 0x01 = LASER active.
BAT_LASER = "01017400AD000000040DEFAE1C0A74AD040DF0000180"
# (probe frame, expected cumulative shots) — the 12-frame cadence (0x18).
CASES = [(1, 1), (12, 1), (13, 2)]


def probe_shots(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_laser.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 BATTY_AUTO_FIRE=1 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_LASER} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_laser"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"laser_fire_state=shots([0-9A-Fa-f]{4})_cd([0-9A-Fa-f]{2})",
                  probe.read_text())
    if not m:
        return None
    return int(m.group(1), 16), int(m.group(2), 16)


def main() -> int:
    ok = True
    for frame, eshots in CASES:
        r = probe_shots(frame)
        if r is None:
            print(f"  frame {frame}: NO laser_fire_state in PROBE.TXT [FAIL]")
            ok = False
            continue
        shots, cd = r
        good = (shots == eshots)
        ok = ok and good
        print(f"  frame {frame}: shots={shots} cd=0x{cd:02X} "
              f"[{'PASS' if good else 'FAIL'}] (expect shots={eshots})")
    if ok:
        print("PASS laser_cadence: held-fire shoots every 12 frames "
              "(shots 1/1/2 at f1/12/13) — 0x18 cooldown reset guarded")
        return 0
    print("FAIL laser_cadence")
    return 1


if __name__ == "__main__":
    sys.exit(main())
