#!/usr/bin/env python3
"""Enemy STEERING gate (port vs original ground truth, RNG-dependent).

The descend gate (test_enemy_descend.py) covers the RNG-independent y<8
slide. This covers the y>=8 STEERING leg, which IS RNG-dependent: at spawn
`dir==target==0x10`, so the first `LAA7D` turn hits `delta==0` and repicks
`target = random_number & $3F`; the enemy then steers `dir` toward that
target each 4-frame tick. Getting the target right needs the byte-exact
RNG walk (the per-frame tick, ON by default) seeded to the L3 f0 value.

Ground truth (ZEsarUX, scripts/capture_enemy_flight.py): the original turns
`dir` 0x11->0x12->0x13 over f16/f20/f24 (x drifts left 167->167->165) as it
heads to its repicked target. With the WRONG/stale RNG seed the port
steered the wrong way (dir 0x0E->0x0C, x drifting right) — this gate is what
caught that, so it guards against any RNG-walk / steering regression.

Bakes the byte-correct f0 seed (RANDOM=3793, RANDOM_SEED=962A; per-frame
tick is the default now), the fresh enemy descriptor, AND the counter_misc
phase pin (BATTY_REPLAY_COUNTER=2, see SEED_COUNTER below), then probes
object_enemy at f16/f20/f24 and asserts x, y, AND dir EXACTLY against the
GT. The pin removes the old run-to-run jitter (the steer cadence gates on
the GLOBAL counter's &3 phase, formerly boot-wall-clock random — boots
landing on phase 0 read dir one steer-step ahead and flaked this test).
With the phase pinned the run is byte-deterministic (validated 3/3
identical incl. exact x), so the former +-1 x slack is gone.

ZEsarUX-free (port-only); needs QEMU + mtools. See notes/enemy-movement.md
and notes/rng-model.md.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

SEED_RANDOM = "3793"
SEED_RANDOM_SEED = "962A"
# Pin the global frame counter (counter_misc) phase at the WAIT_KEY release.
# The steer cadence is gated on the GLOBAL counter (&3), whose phase at the
# aligned start was boot-wall-clock roulette — the 4-frame turn slid across
# a probe frame run-to-run and dir read one step early (the old flake;
# phase 0 reproduces it deterministically: dir 0x12/0x13/0x14). Swept all 4
# phases: 1/2/3 reproduce the ZEsarUX GT dirs; phase 2 also matches x
# EXACTLY (167/167/165) and puts the probe frames (16/20/24) maximally far
# from the turn boundaries (turns at f=2,6,...,22). Overridable for sweeps.
SEED_COUNTER = os.environ.get("BATTY_REPLAY_COUNTER_OVERRIDE", "2")
BAT_OBJECT  = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BALL_OBJECT = "02006C004E001F03020CEEF008076C4E020C0000008C"
FRESH_ENEMY = "0900A80001001001030FDA35180CA801030FF0701000"

# (probe frame, expected x, y, dir) — the original's steering ground truth.
CASES = [(16, 167, 16, 0x11), (20, 167, 20, 0x12), (24, 165, 24, 0x13)]


def probe_enemy(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_steer.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM={SEED_RANDOM} "
        f"BATTY_REPLAY_RANDOM_SEED={SEED_RANDOM_SEED} "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} BATTY_REPLAY_BALL_STUCK=0 "
        f"BATTY_REPLAY_BALL_OBJECT={BALL_OBJECT} "
        f"BATTY_REPLAY_ENEMY_OBJECT={FRESH_ENEMY} "
        f"BATTY_REPLAY_COUNTER={SEED_COUNTER} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_steer"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"object_enemy=([0-9A-Fa-f]+)", probe.read_text())
    return bytes.fromhex(m.group(1)) if m else None


def main() -> int:
    ok = True
    for frame, ex, ey, ed in CASES:
        b = probe_enemy(frame)
        if b is None:
            print(f"  frame {frame}: NO enemy in PROBE.TXT [FAIL]")
            ok = False
            continue
        x, y, d = b[2], b[4], b[6]
        good = (x == ex and y == ey and d == ed)
        ok = ok and good
        print(f"  frame {frame}: x={x} y={y} dir=0x{d:02X} "
              f"[{'PASS' if good else 'FAIL'}] (expect x={ex} y={ey} dir=0x{ed:02X})")
    if ok:
        print("PASS enemy_steer: port steering matches the original "
              "(dir 0x11->0x12->0x13, x 167->167->165) — RNG-dependent, GT-validated")
        return 0
    print("FAIL enemy_steer")
    return 1


if __name__ == "__main__":
    sys.exit(main())
