#!/usr/bin/env python3
"""In Double Play bat 2 can catch a falling bonus, and the 400 is its own.

`get_bonus` ($A67B) has the same fall-through shape as LAB1F:

    LD IY,object_bat_1 / CALL obj_compare_2pix / JR C,LA67B_0
    LD A,(game_mode) / CP $02 / RET NZ
    LD IY,object_bat_2 / CALL obj_compare / RET NC
    CALL bonus_flag_swap / CALL LA67B_0 / JP bonus_flag_swap

bat 1 first, bat 2 only in mode $02 and only when bat 1 missed. The port
tested bat 1 alone, so a bonus falling on player 2's half of the court
was caught by nobody — it fell straight through the bat and off the
bottom.

`LA67B_1` then sets `need_change_player` from the CATCHING bat's x, so
the 400 points follow the bat that got it. The port passed `BAT_X`
unconditionally, which was correct only for as long as bat 2 could
never catch anything.

### The scenario

Mode 2, a bonus seeded above bat 2's resting place ($B0..$CC) with
nothing else in play:

  subject  mode 2 -> caught: bonus.active goes 0, and 2UP has 400
  control  mode 0 -> no bat 2: it falls past, and nobody scores

`BATTY_HIDE_BALL` keeps the ball out of it, so the only thing that can
change a score is the bonus.

### What this does NOT cover, and it is the bigger half

The EFFECT still lands on both bats. `set_bat_bonus` writes both
`bonus_applied` bytes, and the width/laser state (`bat.extra_px`,
`bat.extra_target`, `bat.big_ticks`) are bat-1 globals with nowhere to
put bat 2's. The original keeps them apart with `bonus_flag_swap`
around the bat-2 call — swapping `bonus_flag` with `bonus_flag_copy` so
`LA67B_0` operates on the catching bat's state throughout.

So this gate pins WHO CATCHES and WHO IS PAID, not who gets the effect.
Splitting the effect needs per-bat width and laser state, which is the
open remainder of WS3.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-dpbon.img")
FRAME = 30

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BONUS_X, BONUS_Y = 186, 158      # above bat 2's body at $B0..$CC, y=$AD
#   The fall is SLOW (motion_accel_step from a standing start): a
#   bonus seeded at y=150 had moved 6 px in 20 frames. Seed it a
#   short drop away and give it 30.


def probe(mode: str):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_dpbon.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 BATTY_LEVEL=1 "
        f"BATTY_GAME_MODE={mode} BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 "
        f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_COUNTER=0 "
        f"BATTY_HIDE_BALL=1 BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT} "
        f"BATTY_REPLAY_BONUS=7,{BONUS_X},{BONUS_Y} "
        f"BATTY_VISUAL_PROBE_FRAMES={FRAME}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(FRAME), "--wait-key",
                    "--out", "build/tl_dpbon"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    t = out.read_text()
    m = re.search(r"scores=(\d+)_(\d+)_own", t)
    b = re.search(r"bonus_state=active([0-9A-Fa-f]{2})_type[0-9A-Fa-f]{2}"
                  r"_x([0-9A-Fa-f]{2})_y([0-9A-Fa-f]{2})", t)
    if not m or not b:
        raise SystemExit("FAIL: PROBE.TXT has no scores or bonus_state row")
    return (int(m.group(1)), int(m.group(2)),
            int(b.group(1), 16), int(b.group(3), 16))


def main() -> int:
    ok = True

    got = probe("2")
    if got is None:
        print("  mode 2: NO PROBE.TXT [FAIL]")
        return 1
    s1, s2, act, by = got
    good = (s2 == 5400 and s1 == 0)
    ok = ok and good
    print(f"  mode 2 (Double Play): 1UP={s1} 2UP={s2} bonus(act={act},y={by}) "
          f"[{'PASS' if good else 'FAIL'}] (expect 5400 on 2UP alone — "
          f"the 400 catch award plus SCORE_5K, both to the catching bat)")

    got = probe("0")
    if got is None:
        print("  mode 0: NO PROBE.TXT [FAIL]")
        return 1
    s1, s2, act, by = got
    good = (s1 == 0 and s2 == 0)
    ok = ok and good
    print(f"  mode 0 (1 Player):    1UP={s1} 2UP={s2} bonus(act={act},y={by}) "
          f"[{'PASS' if good else 'FAIL'}] (expect nothing — there is no "
          f"bat 2, and bat 1 at $74 is nowhere near it)")

    if ok:
        print("PASS double_play_bonus_catch: bat 2 catches the bonus and "
              "the 400 lands on 2UP")
        return 0
    print("FAIL double_play_bonus_catch")
    return 1


if __name__ == "__main__":
    sys.exit(main())
