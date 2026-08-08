#!/usr/bin/env python3
"""Bonus-drop economy gate (RNG-gated, deterministic via force-spawn).

When a brick is destroyed the original rolls a bonus drop iff
`(random_number_hi & 0x0F) < 5` — a 5/16 (~31%) chance (the test at $A2CC,
read-current i.e. NON-advancing). This drop ECONOMY had no standing gate;
the midgame gate only happened to observe a no-drop seed.

To isolate the drop DECISION from brick-hit timing + the per-frame RNG tick,
a BATTY_FORCE_SPAWN_BONUS hook calls try_spawn_bonus() once at level entry
with the freshly-baked RNG (no frames elapsed). With rng_perframe ON (the
default) rng_sample() returns the un-advanced random_number, so random_d
(the hi byte of BATTY_REPLAY_RANDOM) directly decides the drop. The expected
result is computed here FROM FIRST PRINCIPLES — `(random_d & 0x0F) < 5` —
not from the C code, so this validates the port implements the 5/16 rule
(and pins the threshold: 4 -> drop, 5 -> no drop).

Ball hidden + no-ball-death suppressed so the forced bonus is the only one;
probe `bonus_state` at f1 (still falling) and assert active == expected.

ZEsarUX-free (port-only); needs QEMU + mtools. See src/main.cpp try_spawn_bonus
/ rng_sample and notes/rng-model.md.
"""
import re
import subprocess
import sys
from pathlib import Path

from test_visual import test_floppy

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = test_floppy()

# random_number hi byte (BATTY_REPLAY_RANDOM is D:E, so the first 2 hex are
# random_d). Threshold boundary: low nibble 0..4 drop, 5..15 no drop.
CASES = ["3093", "3493", "3593", "3F93"]


def probe_drop(rnd: str):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_drop.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_RANDOM={rnd} BATTY_REPLAY_RANDOM_SEED=962A "
        f"BATTY_FORCE_SPAWN_BONUS=1 "
        f"BATTY_VISUAL_PROBE_FRAMES=1"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", "1", "--wait-key",
                    "--out", "build/tl_drop"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"bonus_state=active([0-9A-Fa-f]{2})_type", probe.read_text())
    return int(m.group(1), 16) if m else None


def main() -> int:
    ok = True
    for rnd in CASES:
        random_d = int(rnd[:2], 16)
        expect_drop = (random_d & 0x0F) < 5          # documented 5/16 rule
        active = probe_drop(rnd)
        if active is None:
            print(f"  RANDOM={rnd}: NO bonus_state in PROBE.TXT [FAIL]")
            ok = False
            continue
        good = (active == 1) == expect_drop
        ok = ok and good
        print(f"  RANDOM={rnd} (d=0x{random_d:02X}, d&0x0F={random_d & 0x0F}): "
              f"active={active} [{'PASS' if good else 'FAIL'}] "
              f"(expect drop={expect_drop})")
    if ok:
        print("PASS bonus_drop: drop fires iff (random_d & 0x0F) < 5 "
              "(threshold 4->drop, 5->no) — 5/16 economy guarded")
        return 0
    print("FAIL bonus_drop")
    return 1


if __name__ == "__main__":
    sys.exit(main())
