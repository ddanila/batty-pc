#!/usr/bin/env python3
"""Enemy wing-flap sprite-animation gate (port-only, RNG-independent).

The alien's sprite cycles through an 8-step ping-pong (spr_bird_frames /
spr_ufo_frames) driven by handling_bird_obj: once the alien finishes its
y<8 entry slide, `misc_12` ticks each frame and `sprite_num` advances
(+1)&7 every 4th frame. The RENDER maps sprite_num through the ping-pong
table. This visible animation was code-comparison-verified but previously
ungated — earlier notes assumed it hit the enemy gates' ±1-frame boot
jitter, but that jitter came from the MOVING ball in those gates; with the
ball HIDDEN the enemy is fully frame-deterministic (x is exact, not ±1).

Bakes the steer-gate's fresh bird (sprite_set=0x09, misc_12=0xF0, y0=1) but
with the ball hidden + no-ball-death suppressed, then probes object_enemy
and reads sprite_num (byte 1) at mid-plateau frames, asserting the 4-frame
cadence:
  f8 -> 0, f12 -> 1, f16 -> 2, f20 -> 3, f24 -> 4.
Mid-plateau probe points (each plateau is 4 frames wide) give ±1 robustness
for free; the scenario has proven exact in practice.

ZEsarUX-free (port-only); needs QEMU + mtools. See scripts/test_enemy_steer.py
(same bake, moving ball) and src/main.cpp handling_bird_obj.
"""
import re
import subprocess
import sys
from pathlib import Path

from test_visual import test_floppy

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = test_floppy()

FRESH_ENEMY = "0900A80001001001030FDA35180CA801030FF0701000"
# (probe frame, expected sprite_num) — 4-frame ping-pong cadence.
CASES = [(8, 0), (12, 1), (16, 2), (20, 3), (24, 4)]


def probe_enemy(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_anim.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_ENEMY_OBJECT={FRESH_ENEMY} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_anim"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"object_enemy=([0-9A-Fa-f]+)", probe.read_text())
    if not m:
        return None
    b = bytes.fromhex(m.group(1))
    return b[0], b[1]            # sprite_set, sprite_num


def main() -> int:
    ok = True
    for frame, esn in CASES:
        r = probe_enemy(frame)
        if r is None:
            print(f"  frame {frame}: NO enemy in PROBE.TXT [FAIL]")
            ok = False
            continue
        sset, sn = r
        active = (sset & 0x80) == 0 and (sset & 0x7F) == 0x09
        good = (active and sn == esn)
        ok = ok and good
        print(f"  frame {frame}: sprite_set=0x{sset:02X} sprite_num={sn} "
              f"[{'PASS' if good else 'FAIL'}] (expect bird active, sprite_num={esn})")
    if ok:
        print("PASS enemy_anim: wing-flap advances every 4 frames "
              "(sprite_num 0/1/2/3/4 at f8/12/16/20/24) — handling_bird_obj guarded")
        return 0
    print("FAIL enemy_anim")
    return 1


if __name__ == "__main__":
    sys.exit(main())
