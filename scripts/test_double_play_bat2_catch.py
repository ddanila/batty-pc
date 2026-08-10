#!/usr/bin/env python3
"""With MAGNET up, bat 2 catches the ball and holds it on ITS bat.

`LAB1F` tries bat 1 and falls through to bat 2 in mode $02, and the
catch branch (`LAB1F_1..3`, bonus $03) belongs to whichever bat the ball
actually met. The port had the branch on bat 1 only, so a ball arriving
on bat 2's half bounced off a magnet bat that should have held it.

Two things had to exist first, and both landed as their own commits:
the stuck fields being per-BALL (`cce6483`), and the eight stuck
functions taking a BAT (this one). What is new here is one field —
`ball.stuck_bat[]` — because a held ball rides the bat that caught it,
and that cannot be derived: bat 2 is not simply "the bat on the right"
once a court clamp has moved it, and the ball's x IS the bat's.

### The scenario

Mode 2, MAGNET seeded on the bats, ball aimed down-right at bat 2's
resting place, exactly the geometry `test-double-play-bat2` uses for the
deflection case:

  subject  a CATCH bonus seeded ALREADY OVERLAPPING bat 2 at (186,168),
           so it is picked up on frame 1 -> the ball, arriving around
           frame 12, STOPS on bat 2: y is the caught rest height and it
           does not move between the two probe frames
  control  no bonus -> the ball deflects UP and keeps moving

The bonus used to be dropped on BAT 1, because `set_bat_bonus` wrote
both bats and the code reached bat 2 for free. That mirror was the open
divergence, and this docstring said so: "when ownership splits, the seed
has to drop the bonus on bat 2 instead."

It split on 2026-08-10 and this gate failed in the same sweep, exactly
there. Predicting a test's future failure in its own docstring is worth
the two lines it costs — the fix was mechanical instead of a hunt.

Reseeding took two goes: dropped at y=158 the bonus is still falling at
frame 12 (the fall accelerates from a standing start and covers about
six pixels in twenty frames), so the MAGNET was not up yet when the ball
arrived and the ball simply bounced. Seeded at 168 it overlaps the bat
immediately and is caught on frame 1.

The two-frame comparison is the point. A single frame cannot tell a
caught ball from one that happens to be at the rest height on its way
past, and the first draft of this gate could not either.

### Why the ball's x pins WHICH bat

A caught ball sits at `bat_x + offset`, so a ball held by bat 2 is at
x >= $80 and one held by bat 1 is below it. If `stuck_bat` were ignored
and the ball rode bat 1 regardless, it would jump left across the
divider on the frame after the catch — which is exactly what the
un-threaded code did, and what the x assertion below catches.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-b2catch.img")

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
DIVIDER = 0x80
CAUGHT_REST_Y = 167          # BAT_Y - BALL_H_PX + 1, LAB1F_3's $A7


def ball_seed() -> str:
    b = bytearray(bytes.fromhex(
        "02006C004E001F03020CEEF008076C4E020C0000008C"))
    b[2] = 0xB8      # x = 184, over bat 2's body (176..207)
    b[4] = 0xA0      # y = 160, above the bat top
    b[6] = 0x08      # down-right; a diagonal, see test_double_play_bat2
    return b.hex().upper()


def probe(magnet: bool, frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_b2catch.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 BATTY_GAME_MODE=2 "
        f"BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_COUNTER=0 BATTY_REPLAY_BALL_STUCK=0 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT} "
        f"BATTY_REPLAY_BALL_OBJECT={ball_seed()} "
        f"{'BATTY_REPLAY_BONUS=5,186,168 ' if magnet else ''}"
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_b2catch"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    m = re.search(r"object_ball_1=([0-9A-Fa-f]+)", out.read_text())
    if not m:
        raise SystemExit("FAIL: PROBE.TXT has no object_ball_1 row")
    b = bytes.fromhex(m.group(1))
    return b[2], b[4]           # x, y


def main() -> int:
    ok = True

    a = probe(True, 12)
    b = probe(True, 16)
    if a is None or b is None:
        print("  MAGNET: NO PROBE.TXT [FAIL]")
        return 1
    held = (a == b and a[1] == CAUGHT_REST_Y and a[0] >= DIVIDER)
    ok = ok and held
    print(f"  MAGNET on  : f12=({a[0]},{a[1]}) f16=({b[0]},{b[1]}) "
          f"[{'PASS' if held else 'FAIL'}] (expect identical, y="
          f"{CAUGHT_REST_Y} caught-rest, x>=${DIVIDER:02X} — held ON BAT 2, "
          f"not dragged back to bat 1)")

    c = probe(False, 12)
    d = probe(False, 16)
    if c is None or d is None:
        print("  no bonus: NO PROBE.TXT [FAIL]")
        return 1
    moving = (c != d and d[1] < CAUGHT_REST_Y)
    ok = ok and moving
    print(f"  MAGNET off : f12=({c[0]},{c[1]}) f16=({d[0]},{d[1]}) "
          f"[{'PASS' if moving else 'FAIL'}] (expect moving and rising — "
          f"bat 2 deflects it instead)")

    if ok:
        print("PASS double_play_bat2_catch: bat 2 catches the ball and "
              "holds it on its own bat")
        return 0
    print("FAIL double_play_bat2_catch")
    return 1


if __name__ == "__main__":
    sys.exit(main())
