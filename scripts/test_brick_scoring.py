#!/usr/bin/env python3
"""Per-row brick scoring gate (port-only, deterministic).

Destroying a brick awards points_table[row] — top rows score more — doubled
when the brick's colour nibble is >= 6 (the $AFD6 JP C, add_points_to_score
metal-brick test). points_table = {120,110,100,90,80,70,60,50,40,30,20,10}
for rows 0..11. This per-row + x2-colour economy had no standing gate (the
midgame gate only checks a fixed total).

Uses two hooks: BATTY_FORCE_BRICK plants a known single-hit brick (value
0x1X = bit4 set, colour X) at a chosen cell, and BATTY_REPLAY_BULLET bakes a
bullet just below it so it rises into that exact cell on frame 1 (before
reaching any other row), destroying it. score starts at 0 on a fresh L3
entry, so the probed score after the hit equals the awarded points. Expected
values are computed here from the documented points_table + x2 rule, not the
C scoring code.

  row 0  colour 5 -> 120      row 5  colour 5 -> 70
  row 11 colour 5 -> 10       row 3  colour 6 -> 90*2 = 180

ZEsarUX-free (port-only); needs QEMU + mtools. See src/main.cpp
step_bullet_one (brick-hit scoring) / points_table.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

POINTS = [120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20, 10]
COL = 5
# (row, colour nibble) -> expected score = POINTS[row] * (2 if colour>=6 else 1)
CASES = [(0, 5), (5, 5), (11, 5), (3, 6)]


def probe_score(row: int, colour: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_score.txt"
    probe.unlink(missing_ok=True)
    value = 0x10 | (colour & 0x0F)          # bit4 set = single-hit, colour X
    bullet_x = 8 + COL * 16 + 4
    bullet_y = 32 + row * 8 + 8             # one cell below; -6 on f1 lands in row
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_FORCE_BRICK={COL},{row},{value} "
        f"BATTY_REPLAY_BULLET={bullet_x},{bullet_y} "
        f"BATTY_VISUAL_PROBE_FRAMES=2"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", "2", "--wait-key",
                    "--out", "build/tl_score"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"^score=(\d+)", probe.read_text(), re.M)
    return int(m.group(1)) if m else None


def main() -> int:
    ok = True
    for row, colour in CASES:
        expected = POINTS[row] * (2 if colour >= 6 else 1)
        sc = probe_score(row, colour)
        if sc is None:
            print(f"  row {row} colour {colour}: NO score in PROBE.TXT [FAIL]")
            ok = False
            continue
        good = (sc == expected)
        ok = ok and good
        print(f"  row {row} colour {colour}: score={sc} "
              f"[{'PASS' if good else 'FAIL'}] (expect {expected})")
    if ok:
        print("PASS brick_scoring: per-row points_table + x2 metal-colour award "
              "match (120/70/10 by row, 90*2 for colour 6) — scoring guarded")
        return 0
    print("FAIL brick_scoring")
    return 1


if __name__ == "__main__":
    sys.exit(main())
