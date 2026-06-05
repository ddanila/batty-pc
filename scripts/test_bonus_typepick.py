#!/usr/bin/env python3
"""Bonus TYPE-pick gate (port-only, deterministic).

After the drop gate fires, the original picks the bonus TYPE: loop calling
random_generate, idx = random_hi & 0x0F, code = bonus_table[idx], with
per-type exclusions (dup of bat.bonus_applied, SLOW if a ball is already at
min speed, LIFE if already dropped, ROCKET if in flight / rarer past round
6), then map to a supported type. This is the last ungated bonus-system
piece.

Gating the EXACT type per seed would require reimplementing the RNG walk
(circular vs the C, or needs ZEsarUX GT). Instead this validates the pick
LOGIC non-circularly via properties that must hold across many seeds, plus
a regression pin:

  PROPERTIES (parity validation, derived from the documented rules):
    - every forced drop resolves to a VALID supported type (0..9, the loop
      always terminates with a mapped bonus — never UNSUPPORTED/no-spawn);
    - the picked type is NEVER SLOW (port type 1): at level entry the ball
      is at base speed, so the SLOW exclusion ($A...: code 0x04 skipped when
      ball speed <= base) must suppress it — a regression removing that
      exclusion would surface SLOW across these seeds;
    - the table is non-degenerate: >= 3 distinct types appear.
  REGRESSION PIN (golden, from the current byte-exact build): the exact
    type per seed — catches any RNG-walk / table / mapping change. The RNG
    itself is independently gated by test-rng-walk, so a golden change here
    means the table/exclusions changed, not the RNG.

Forces a drop (RANDOM hi nibble < 5) with rng_perframe ON and varies the
seed to vary the pick; ball hidden + no-ball-death suppressed.

ZEsarUX-free (port-only); needs QEMU + mtools. See src/main.c try_spawn_bonus
and test_bonus_drop.py (the drop-decision side).
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

SLOW = 1   # BONUS_TYPE_SLOW — excluded at base ball speed.
# (seed, golden expected port type) — RANDOM fixed at 3093 (hi nibble 0 -> drop).
CASES = [
    ("8000", 0), ("8456", 2), ("8ABC", 9), ("9000", 8),
    ("9123", 9), ("9800", 0), ("9F00", 2), ("8D4A", 9),
]


def probe_type(seed: str):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_tp.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_RANDOM=3093 BATTY_REPLAY_RANDOM_SEED={seed} "
        f"BATTY_FORCE_SPAWN_BONUS=1 BATTY_VISUAL_PROBE_FRAMES=1"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", "1", "--wait-key",
                    "--out", "build/tl_tp"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"bonus_state=active([0-9A-Fa-f]{2})_type([0-9A-Fa-f]{2})",
                  probe.read_text())
    if not m:
        return None
    return int(m.group(1), 16), int(m.group(2), 16)


def main() -> int:
    ok = True
    seen = set()
    for seed, golden in CASES:
        r = probe_type(seed)
        if r is None:
            print(f"  seed={seed}: NO bonus_state in PROBE.TXT [FAIL]")
            ok = False
            continue
        active, t = r
        seen.add(t)
        valid = (active == 1 and 0 <= t <= 9 and t != SLOW)
        pinned = (t == golden)
        good = valid and pinned
        ok = ok and good
        flags = []
        if not valid:
            flags.append("INVALID/SLOW/no-drop")
        if not pinned:
            flags.append(f"pin!=0x{golden:02X}")
        print(f"  seed={seed}: active={active} type={t} "
              f"[{'PASS' if good else 'FAIL'}]"
              + (f" ({', '.join(flags)})" if flags else ""))
    if len(seen) < 3:
        print(f"  table degenerate: only {len(seen)} distinct types {sorted(seen)} [FAIL]")
        ok = False
    if ok:
        print(f"PASS bonus_typepick: all drops resolve to valid non-SLOW types, "
              f"{len(seen)} distinct {sorted(seen)} — pick logic + table guarded")
        return 0
    print("FAIL bonus_typepick")
    return 1


if __name__ == "__main__":
    sys.exit(main())
