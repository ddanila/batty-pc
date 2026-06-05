#!/usr/bin/env python3
"""Ball speed-up ramp gate (port-only, deterministic).

The ball accelerates over a level: ball_speed_ramp_tick() increments a
counter once per 8 frames (counter_misc & 7 == 0) and, when it reaches 0x94
(148), resets it and bumps every active ball's speed by 1, capped at 6
(handling_ball LA27E_22 / get_bonus LA67B_7). The full climb is ~1184
frames per step — too slow to frame-step directly — so this gate SEEDS the
counter near the threshold (BATTY_REPLAY_BALL_RAMP) to observe one bump in a
few frames. (Earlier notes wrongly called this ungateable; seeding the
counter makes it reachable.)

Bakes a moving ball (BATTY_REPLAY_BALL_OBJECT, BALL_STUCK=0) + a seeded
ramp, no-ball-death suppressed, and probes object_ball_1.speed at f12 (well
past the first 8-frame tick). The bump fires once within the first 8 frames,
so the speed VALUE is robust to the run-to-run boot-phase jitter:

  ramp 0x93 (one tick from 0x94), base spd 3 -> 4   (bump fires)
  ramp 0x00 (far from threshold), base spd 3 -> 3   (no bump in 12 frames)
  ramp 0x93, base spd 6                     -> 6   (cap holds, no 7)

Expected from the documented mechanic (threshold 0x94, +1, cap 6), not the
C code. ZEsarUX-free (port-only); needs QEMU + mtools. See src/main.c
ball_speed_ramp_tick.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

FLOPPY = "build/batty-test.img"


def ball_obj(speed: int) -> str:
    # steer-gate moving ball with byte 0x07 (speed) overridden.
    b = bytearray.fromhex("02006C004E001F03020CEEF008076C4E020C0000008C")
    b[7] = speed
    return b.hex().upper()


# (ramp seed, base speed, expected speed at f12, label)
CASES = [
    (0x93, 3, 4, "seed 0x93 -> bump 3->4"),
    (0x00, 3, 3, "seed 0x00 -> no bump (3)"),
    (0x93, 6, 6, "seed 0x93 at cap -> stays 6"),
]


def probe_speed(ramp: int, base_speed: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_ramp.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_BALL_OBJECT={ball_obj(base_speed)} "
        f"BATTY_REPLAY_BALL_STUCK=0 BATTY_REPLAY_BALL_RAMP={ramp} "
        f"BATTY_VISUAL_PROBE_FRAMES=12"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", "12", "--wait-key",
                    "--out", "build/tl_ramp"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"speed_ramp_state=ramp[0-9A-Fa-f]{4}_spd([0-9A-Fa-f]{2})",
                  probe.read_text())
    return int(m.group(1), 16) if m else None


def main() -> int:
    ok = True
    for ramp, base, expected, label in CASES:
        spd = probe_speed(ramp, base)
        if spd is None:
            print(f"  {label}: NO speed_ramp_state in PROBE.TXT [FAIL]")
            ok = False
            continue
        good = (spd == expected)
        ok = ok and good
        print(f"  {label}: spd={spd} [{'PASS' if good else 'FAIL'}] (expect {expected})")
    if ok:
        print("PASS ball_speed_ramp: 0x94 threshold bumps speed +1 (cap 6) "
              "on the 8-frame cadence — ball_speed_ramp_tick guarded")
        return 0
    print("FAIL ball_speed_ramp")
    return 1


if __name__ == "__main__":
    sys.exit(main())
