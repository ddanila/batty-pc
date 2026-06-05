#!/usr/bin/env python3
"""Bonus catch->ACTUAL-effect gate (port-only, deterministic).

test-bonus-effects checks the bat.bonus_applied code each catch writes;
this checks the visible GAMEPLAY effect each catch produces:
  - MULTI_BALL (port 9)  -> two extra balls spawn (ball2_active, ball3_active)
  - BIG_BAT    (port 2)  -> bat widen target = BAT_BIG_EXTRA_PX (8)
  - BIG_BALL   (port 3)  -> big_ball timer armed
  - LIFE       (port 0)  -> lives++ (3 -> 4)

Bakes a bonus of each port type just above the bat (BATTY_REPLAY_BONUS=
type,x,y) so it is caught on f1, ball hidden + no-ball-death suppressed,
then probes a new `effects_state` line at f2 and asserts the effect fired.
Expected values are the documented effect constants (8 px widen, lives
3->4), not derived from the C effect code. Deterministic + non-circular.

ZEsarUX-free (port-only); needs QEMU + mtools. See src/main.c bonus_apply
(BONUS_TYPE_* switch) and test_bonus_effects.py (the bonus_applied side).
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

# (port type, field, expected, name)
CASES = [
    (9, "b2", 1, "MULTI_BALL -> ball2 spawns"),
    (9, "b3", 1, "MULTI_BALL -> ball3 spawns"),
    (2, "xtgt", 8, "BIG_BAT -> widen target 8"),
    (3, "bball", 1, "BIG_BALL -> timer armed"),
    (0, "lives", 4, "LIFE -> lives 3->4"),
]


def probe_effects(port_type: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_eff2.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_BONUS={port_type},118,167 "
        f"BATTY_VISUAL_PROBE_FRAMES=2"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", "2", "--wait-key",
                    "--out", "build/tl_eff2"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"effects_state=b2([0-9A-Fa-f]{2})_b3([0-9A-Fa-f]{2})"
                  r"_xtgt([0-9A-Fa-f]{2})_bball([0-9A-Fa-f]{2})"
                  r"_lives([0-9A-Fa-f]{2})", probe.read_text())
    if not m:
        return None
    return {"b2": int(m.group(1), 16), "b3": int(m.group(2), 16),
            "xtgt": int(m.group(3), 16), "bball": int(m.group(4), 16),
            "lives": int(m.group(5), 16)}


def main() -> int:
    ok = True
    # Probe each distinct type once, reuse for both MULTI_BALL fields.
    cache = {}
    for port_type, field, expected, name in CASES:
        if port_type not in cache:
            cache[port_type] = probe_effects(port_type)
        st = cache[port_type]
        if st is None:
            print(f"  {name}: NO effects_state in PROBE.TXT [FAIL]")
            ok = False
            continue
        got = st[field]
        good = (got == expected)
        ok = ok and good
        print(f"  {name}: {field}={got} [{'PASS' if good else 'FAIL'}] "
              f"(expect {expected})")
    if ok:
        print("PASS bonus_effects2: catches produce their gameplay effects "
              "(multi-ball spawn, bat widen, big-ball timer, extra life)")
        return 0
    print("FAIL bonus_effects2")
    return 1


if __name__ == "__main__":
    sys.exit(main())
