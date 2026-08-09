#!/usr/bin/env python3
"""In Double Play bat 2 kills the alien too, and scores for its own side.

`kill_enemy_by_bat` ($A4B8) is called from `handling_bat`, and in mode
$02 `handling_bat` runs for BOTH bats:

    LD IX,object_bat_1 / CALL handling_bat
    ...
    LD IX,object_bat_2 / CALL handling_bat

There is no bonus condition on the kill — the routine checks only that
an alien exists and is not already exploding, then `obj_compare_2pix`
against the bat. Any bat touching an alien destroys it.

The port checked bat 1 only, so an alien drifting over bat 2's half of
the court flew through it.

### The 350 points, and the three other paths that were also wrong

`add_points_to_score` credits the side that `need_change_player` names,
and the four routines reaching `kill_enemy` each set it from a
DIFFERENT thing:

    handling_bat      (IX+$02) & $80   the BAT's x
    LA67B_1  (bonus)  (IY+$02) & $80   the CATCHING BAT's x
    handling_bullet   (IX+$02) & $80   the BULLET's x
    handling_ball     (IX+$12) & $80   the ball's OWNER bit, not its x

The port passed `BAT_X` to all four, which is right for one of them.
Fixed with the same commit; the bullet's brick points were mis-credited
the same way (they used the ball's owner) and are fixed too.

Only the bat-2 half is gated here. The other three are unreachable from
a seeded scenario without also porting bonus ownership and a bat-2
laser, which are open WS3 items — so they are a code fix with a source
citation rather than a measured one, and this docstring is where that
is admitted rather than left to be discovered.

### The scenario

An alien parked on bat 2's body at ($B8, $AD), speed 0, ball held on
bat 1 so nothing else moves:

  subject  mode 2 -> bat 2 exists and touches it: sprite_set becomes
           $0A (exploding) and 2UP gains exactly 350
  control  mode 0 -> no bat 2: the alien is untouched and nobody scores

Bat 1 sits at $74 in both, a long way from $B8, so it cannot be the one
doing the killing in either run.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-dpak.img")
FRAME = 6

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
#      set num  x  xh  y yh dir spd  ws hs buf   wb hb px py ...
#      09  05  B8  00 AD 00  2D  00  03 0F DD74  18 0C B8 AD
ENEMY = "0905B800AD002D00030FDD74180CB8AD030F30703100"

BLAST_SPRITE_SET = 0x0A
ALIEN_POINTS = 350


def probe(mode: str):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_dpak.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 "
        f"BATTY_GAME_MODE={mode} BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 "
        f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_COUNTER=0 "
        f"BATTY_REPLAY_BALL_STUCK=1 BATTY_REPLAY_BAT_OBJECT={BAT} "
        f"BATTY_REPLAY_ENEMY_OBJECT={ENEMY} "
        f"BATTY_VISUAL_PROBE_FRAMES={FRAME}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(FRAME), "--wait-key",
                    "--out", "build/tl_dpak"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    text = out.read_text()
    e = re.search(r"object_enemy=([0-9A-Fa-f]+)", text)
    s = re.search(r"scores=(\d+)_(\d+)_own", text)
    if not e or not s:
        raise SystemExit("FAIL: PROBE.TXT has no object_enemy or scores row")
    return bytes.fromhex(e.group(1))[0], int(s.group(1)), int(s.group(2))


def main() -> int:
    ok = True

    got = probe("2")
    if got is None:
        print("  mode 2: NO PROBE.TXT [FAIL]")
        return 1
    sset, s1, s2 = got
    good = (sset == BLAST_SPRITE_SET and s2 == ALIEN_POINTS and s1 == 0)
    ok = ok and good
    print(f"  mode 2 (Double Play): enemy set=${sset:02X} 1UP={s1} 2UP={s2} "
          f"[{'PASS' if good else 'FAIL'}] (expect $0A exploding, and the "
          f"{ALIEN_POINTS} on 2UP alone — bat 2 killed it on its own side)")

    got = probe("0")
    if got is None:
        print("  mode 0: NO PROBE.TXT [FAIL]")
        return 1
    sset, s1, s2 = got
    good = (sset != BLAST_SPRITE_SET and s1 == 0 and s2 == 0)
    ok = ok and good
    print(f"  mode 0 (1 Player):    enemy set=${sset:02X} 1UP={s1} 2UP={s2} "
          f"[{'PASS' if good else 'FAIL'}] (expect NOT $0A and no score — "
          f"there is no bat 2, and bat 1 at $74 is nowhere near it)")

    if ok:
        print("PASS double_play_alien_kill: bat 2 kills the alien and the "
              "350 lands on 2UP")
        return 0
    print("FAIL double_play_alien_kill")
    return 1


if __name__ == "__main__":
    sys.exit(main())
