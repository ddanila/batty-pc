#!/usr/bin/env python3
"""A MAGNET bat catches a SECONDARY ball, not just the primary.

This is the case PLAN.md WS6 item 2 was opened for, and the reason it
sat deferred: the stuck system was one ball's worth of state written
around bat 1. Three commits made it reachable — the fields became `[3]`,
the functions took a ball, then a bat — and the catch itself is one
branch in `step_extra_ball`, whose bat block used to end "No catch."

The original needs no such branch: it runs ONE `handling_ball` per ball
object, and `LAB1F` does not care which one it is. The port's split into
`step_ball` and `step_extra_ball` is what made the catch primary-only,
so this is the port repaying a divergence it introduced.

### Reproducing MAGNET + TRIPLE, which cannot be caught for

The bat holds ONE bonus code at +$14. MULTI_BALL is $02 and MAGNET is
$03, so catching the second overwrites the first and no sequence of
catches puts both in play.

In the game the pair arises the other way round: the extras OUTLIVE the
code that spawned them, so a bat that picks up MAGNET while they are
still flying can catch them. `BATTY_REPLAY_MULTIBALL=1` spawns the
extras at level entry — directions derived from the primary's seeded dir
— and the seeded CATCH bonus is picked up a frame or two later. Same
order, deterministically.

### The scenario

Level 1, the ball seeded just above the bat heading down so all three
reach it quickly, probing `object_ball_2` at two frames:

  subject  MAGNET up -> ball 2 STOPS at the caught rest height ($A7) and
           does not move between the two frames
  control  no bonus  -> ball 2 deflects and keeps moving

Two frames, not one, for the same reason as `test-double-play-bat2-catch`:
a single sample cannot tell a held ball from one crossing the rest
height on its way past.

### What this does NOT cover

Extras are still tested against bat 1 only — `step_extra_ball`'s bat
block reads `eff_bat_left`/`eff_bat_right` and never consults bat 2. So
in Double Play a secondary cannot be caught (or deflected) by bat 2.
That is a separate gap, left whole rather than half-closed, and named
here so this gate is not mistaken for covering it.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-2ndcatch.img")

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
CAUGHT_REST_Y = 167          # BAT_Y - BALL_H_PX + 1, LAB1F_3's $A7


def ball_seed() -> str:
    b = bytearray(bytes.fromhex(
        "02006C004E001F03020CEEF008076C4E020C0000008C"))
    b[2] = 0x74      # x = 116, straight above the bat
    b[4] = 0x94      # y = 148, a short drop to the bat top at 173
    b[6] = 0x08      # down-right
    return b.hex().upper()


def probe(magnet: bool, frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_2ndcatch.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 BATTY_LEVEL=1 "
        f"BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_COUNTER=0 BATTY_REPLAY_BALL_STUCK=0 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 BATTY_REPLAY_MULTIBALL=1 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT} "
        f"BATTY_REPLAY_BALL_OBJECT={ball_seed()} "
        f"{'BATTY_REPLAY_BONUS=5,118,167 ' if magnet else ''}"
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_2ndcatch"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    m = re.search(r"object_ball_2=([0-9A-Fa-f]+)", out.read_text())
    if not m:
        raise SystemExit("FAIL: PROBE.TXT has no object_ball_2 row")
    b = bytes.fromhex(m.group(1))
    return b[2], b[4]           # x, y


def main() -> int:
    ok = True

    a = probe(True, 20)
    b = probe(True, 26)
    if a is None or b is None:
        print("  MAGNET: NO PROBE.TXT [FAIL]")
        return 1
    held = (a == b and a[1] == CAUGHT_REST_Y)
    ok = ok and held
    print(f"  MAGNET on  : f20=({a[0]},{a[1]}) f26=({b[0]},{b[1]}) "
          f"[{'PASS' if held else 'FAIL'}] (expect identical and y="
          f"{CAUGHT_REST_Y} — ball 2 is HELD on the bat)")

    c = probe(False, 20)
    d = probe(False, 26)
    if c is None or d is None:
        print("  no bonus: NO PROBE.TXT [FAIL]")
        return 1
    moving = (c != d)
    ok = ok and moving
    print(f"  MAGNET off : f20=({c[0]},{c[1]}) f26=({d[0]},{d[1]}) "
          f"[{'PASS' if moving else 'FAIL'}] (expect moving — ball 2 "
          f"bounces off the bat instead)")

    if ok:
        print("PASS secondary_ball_catch: a MAGNET bat holds a secondary "
              "ball, not just the primary")
        return 0
    print("FAIL secondary_ball_catch")
    return 1


if __name__ == "__main__":
    sys.exit(main())
