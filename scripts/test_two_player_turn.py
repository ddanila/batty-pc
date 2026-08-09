#!/usr/bin/env python3
"""In 2-player mode, losing a life hands the turn over.

The original's life-loss path:

    LD A,(lives_1up) / DEC A / LD (lives_1up),A
    JR Z,LBC10_6                    ; out of lives -> the game-over path
    LD A,(game_mode) / DEC A
    CALL Z,current_level_2up_copier ; mode 1 (2 Players) only
    JP LB9E8_1                      ; re-enter the level

and `current_level_2up_copier` exchanges the live 180-cell grid with the
arriving player's level slot, then FALLS THROUGH into `players_swap` —
one call does the grid, the counters and the turn toggle. Its
`lives_2up == 0` guard covers both halves, which is how a solo player
keeps playing. Traced in notes/menu.md.

### The scenario is an A/B on one knob

  both runs  BATTY_REPLAY_LIVES=3 + BATTY_HIDE_BALL=1, so
             handle_no_ball_death fires immediately
  control    BATTY_GAME_MODE=0 (1 Player)  -> the turn never changes
  subject    BATTY_GAME_MODE=1 (2 Players) -> it does
  guard      BATTY_GAME_MODE=1 with BATTY_REPLAY_LIVES_2UP=0 -> it does
             NOT: the original's `LD A,(lives_2up) / AND A / RET Z` is
             what lets a solo player keep playing

STILL NOT COVERED: the LBC10_7 hand-over, where the active player runs
OUT of lives, gets the GAME OVER screen, and the other takes over
anyway. `play_game_over` holds for 178 PIT frames (~3.5 s) and the
capture window ends inside that hold, so PROBE.TXT keeps the
level-ENTRY write from before the death and every counter reads 0.
Measured with BATTY_REPLAY_LIVES=1: `go=0`, i.e. even the "reached the
game-over branch" counter never makes it to the file, while
BATTY_REPLAY_LIVES=2 (which hands over WITHOUT a game over) reports
`life=1` immediately. Adding a probe write before `return ST_TITLE`
did not help for the same reason.

What it needs is a way to cut the hold short under the harness, not
another counter. See PLAN.md WS2.

### Why counters and not `active_player`

The first attempt at the game-over case asserted `active_player` and
could not see the feature at all. `PROBE.TXT` is rewritten at every
level entry, and these scenarios die repeatedly, so a probe read at any
moment reports whoever happened to enter LAST — with
`BATTY_REPLAY_LIVES=1` both a working and a broken build reported
`player00`. The case was removed rather than left to pass on whichever
parity the frame count landed on.

`turn_changes_life` / `turn_changes_over` accumulate instead, and
survive every later probe write. Same instrument, same reason, as
`enemy_repicks`. `active_player` is still asserted where it IS stable —
the mode-0 control and the guard case, neither of which re-enters.

The third case exists because mutating that guard from `lives <= 0` to
`lives < 0` SURVIVED the first version of this gate — every case had
player 2 on a full three lives, so the guard was never asked anything.
BATTY_REPLAY_LIVES_2UP was added to reach it.

Same level, same seed, same frame. The only difference is the mode, so a
difference in the reported turn IS the hand-over, with nothing else to
attribute it to. That shape is borrowed from test-life-loss.

### Why the probe and not a screendump

The two runs look nearly identical: both show a bat explosion and a
re-entered level. What differs is WHOSE turn it is, and the only place
that is visible on screen is the round banner's "PLAYER n" digit, which
is up for about a second during the level intro — the same
catch-a-moving-window problem as the Kinnock egg. `PROBE.TXT` carries
`game_mode=<n>_player<n>`, added for exactly this.

Frame 120, not 6: the bat explosion has to finish before the turn change
is reached, and at frame 6 both runs still report the entry probe. Found
by measuring, after a first attempt read `player00` for both and looked
like the feature simply did not work.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-turn.img")
PROBE_FRAME = 120


def probe(mode: str, extra: str = "", lives: str = "3"):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_turn.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 "
        f"BATTY_GAME_MODE={mode} BATTY_REPLAY_LIVES={lives} "
        f"BATTY_HIDE_BALL=1 {extra} "
        f"BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_COUNTER=0 "
        f"BATTY_VISUAL_PROBE_FRAMES={PROBE_FRAME}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(PROBE_FRAME),
                    "--wait-key", "--out", "build/tl_turn"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    m = re.search(r"game_mode=([0-9A-Fa-f]{2})_player([0-9A-Fa-f]{2})"
                  r"_life(\d+)_over(\d+)", out.read_text())
    if not m:
        raise SystemExit(
            "FAIL: PROBE.TXT has no game_mode line with the hand-over "
            "counters. Those counters ARE this gate: active_player alone "
            "is rewritten at every level entry and cannot distinguish a "
            "working build from a broken one here.")
    return (int(m.group(1), 16), int(m.group(2), 16),
            int(m.group(3)), int(m.group(4)))


def main() -> int:
    ok = True
    # want_player is None where re-entry makes it unstable; the
    # counters carry the assertion there instead.
    for mode, l2, lives, want_player, want_life, want_over, why in (
            ("0", "", "3", 0, 0, 0, "1 Player: the turn cannot change"),
            ("1", "", "3", None, 1, 0, "2 Players: the life loss hands over"),
            ("1", "BATTY_REPLAY_LIVES_2UP=0", "3", 0, 0, 0,
             "2 Players but player 2 is out: the guard holds the turn"),
):
        got = probe(mode, l2, lives)
        if got is None:
            print(f"  mode {mode} {l2}: NO PROBE.TXT [FAIL]")
            ok = False
            continue
        got_mode, got_player, life, over = got
        good = (got_mode == int(mode)
                and (want_player is None or got_player == want_player)
                and (life >= want_life if want_life else life == 0)
                and (over >= want_over if want_over else over == 0))
        ok = ok and good
        expect = (f"life>={want_life}" if want_life else "life=0")
        expect += " " + (f"over>={want_over}" if want_over else "over=0")
        if want_player is not None:
            expect += f" player={want_player}"
        print(f"  mode={mode} lives={lives} {l2 or '-':26s} "
              f"player={got_player} life={life} over={over} "
              f"[{'PASS' if good else 'FAIL'}] ({expect} — {why})")

    if ok:
        print("PASS two_player_turn: a life loss hands over in mode 1 and "
              "does not in mode 0")
        return 0
    print("FAIL two_player_turn")
    return 1


if __name__ == "__main__":
    sys.exit(main())
