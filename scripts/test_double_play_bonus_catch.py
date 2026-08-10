#!/usr/bin/env python3
"""In Double Play bat 2 catches a bonus, is paid for it, and KEEPS it.

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

### The third row: the bonus BYTE stays on the catching bat

`set_bat_bonus` used to write BOTH `bonus_applied` bytes, on the
strength of a comment claiming bat 2 was "the second bat the original
keeps for the rocket flight" and that the two "must not disagree".
Both halves were wrong: `object_bat_2` is player 2's bat, and the
original keeps the bytes deliberately apart — `LA67B_0` runs inside
`bonus_flag_swap` for a bat-2 catch.

So a MAGNET caught by bat 2 must make BAT 2 sticky and leave bat 1
alone. Row three drops a CATCH bonus on bat 2 and reads both bytes.

### What this still does NOT cover

The WIDTH and LASER. `bat.extra_px`, `bat.extra_target` and
`bat.big_ticks` are one bat's worth of state, so BIG_BAT caught by bat 2
now widens NOBODY — guarded, rather than widening bat 1 as it did
before. A wrong bat became a missing one, which is the visible face of
WS3's last open item.

The original separates all of it with `bonus_flag_swap` around every
bat-2 call, which presupposes two copies of everything a bonus touches.
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
# Bat 1's +$14 as the seed leaves it. Derived, not written out: the
# first draft expected $FF ("no bonus") and read $83, which is simply
# what this long-standing shared BAT seed carries. The assertion is that
# bat 2's catch does not CHANGE it, so the baseline has to come from the
# same place the run gets it.
SEED_BAT1_BONUS = bytes.fromhex(BAT)[0x14]
BONUS_X, BONUS_Y = 186, 158      # above bat 2's body at $B0..$CC, y=$AD
#   The fall is SLOW (motion_accel_step from a standing start): a
#   bonus seeded at y=150 had moved 6 px in 20 frames. Seed it a
#   short drop away and give it 30.


def probe(mode: str, btype: int = 7, frame: int = FRAME):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_dpbon.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 BATTY_LEVEL=1 "
        f"BATTY_GAME_MODE={mode} BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 "
        f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_COUNTER=0 "
        f"BATTY_HIDE_BALL=1 BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT} "
        f"BATTY_REPLAY_BONUS={btype},{BONUS_X},{BONUS_Y} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
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
    o1 = re.search(r"object_bat_1=([0-9A-Fa-f]+)", t)
    o2 = re.search(r"object_bat_2=([0-9A-Fa-f]+)", t)
    if not m or not b or not o1 or not o2:
        raise SystemExit("FAIL: PROBE.TXT is missing scores, bonus_state or "
                         "a bat object row")
    return (int(m.group(1)), int(m.group(2)),
            int(b.group(1), 16), int(b.group(3), 16),
            bytes.fromhex(o1.group(1))[0x14],
            bytes.fromhex(o2.group(1))[0x14])


def main() -> int:
    ok = True

    got = probe("2")
    if got is None:
        print("  mode 2: NO PROBE.TXT [FAIL]")
        return 1
    s1, s2, act, by, b1b, b2b = got
    good = (s2 == 5400 and s1 == 0)
    ok = ok and good
    print(f"  mode 2 (Double Play): 1UP={s1} 2UP={s2} bonus(act={act},y={by}) "
          f"[{'PASS' if good else 'FAIL'}] (expect 5400 on 2UP alone — "
          f"the 400 catch award plus SCORE_5K, both to the catching bat)")

    got = probe("0")
    if got is None:
        print("  mode 0: NO PROBE.TXT [FAIL]")
        return 1
    s1, s2, act, by, b1b, b2b = got
    good = (s1 == 0 and s2 == 0)
    ok = ok and good
    print(f"  mode 0 (1 Player):    1UP={s1} 2UP={s2} bonus(act={act},y={by}) "
          f"[{'PASS' if good else 'FAIL'}] (expect nothing — there is no "
          f"bat 2, and bat 1 at $74 is nowhere near it)")

    got = probe("2", 5)          # CATCH/MAGNET, original code $03
    if got is None:
        print("  MAGNET on bat 2: NO PROBE.TXT [FAIL]")
        return 1
    _, _, _, _, b1b, b2b = got
    good = (b2b == 0x03 and b1b == SEED_BAT1_BONUS)
    ok = ok and good
    print(f"  MAGNET on bat 2:      bat1=${b1b:02X} bat2=${b2b:02X} "
          f"[{'PASS' if good else 'FAIL'}] (expect "
          f"${SEED_BAT1_BONUS:02X}/$03 — the catching bat keeps it, and "
          f"bat 1 still holds exactly what the seed gave it. Before the "
          f"split both read $03)")

    got = probe("2", 2, frame=60)    # BIG_BAT, original code $00
    if got is None:
        print("  BIG_BAT on bat 2: NO PROBE.TXT [FAIL]")
        return 1
    _, _, _, _, b1b, b2b = got
    good = (b2b == 0x00 and b1b == SEED_BAT1_BONUS)
    ok = ok and good
    print(f"  BIG_BAT on bat 2:     bat1=${b1b:02X} bat2=${b2b:02X} "
          f"[{'PASS' if good else 'FAIL'}] (expect "
          f"${SEED_BAT1_BONUS:02X}/$00 — BIG_BAT is bat 2's; the WIDTH it "
          f"drives is gated by test-double-play-bat2-width)")

    if ok:
        print("PASS double_play_bonus_catch: bat 2 catches the bonus, is "
              "paid for it, and keeps it to itself")
        return 0
    print("FAIL double_play_bonus_catch")
    return 1


if __name__ == "__main__":
    sys.exit(main())
