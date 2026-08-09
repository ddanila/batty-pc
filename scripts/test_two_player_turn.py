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
NOT covered here: the LBC10_7 hand-over, where the active player runs
OUT of lives, gets the GAME OVER screen, and the other player takes over
anyway. With BATTY_REPLAY_LIVES=1 the death fires on the FIRST frame, so
`visual_checkpoint_tick` never reaches its count and the capture reads
the level-ENTRY probe — the same value whether the hand-over happened or
not. Measured: lives=1 with and without BATTY_REPLAY_LIVES_2UP=1 both
report `player00`, indistinguishable from no hand-over at all.

Reaching it needs a checkpoint that survives the game-over hold, or a
knob that delays the first death. Written down in PLAN.md rather than
approximated with a case that would pass for the wrong reason.

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
    m = re.search(r"game_mode=([0-9A-Fa-f]{2})_player([0-9A-Fa-f]{2})",
                  out.read_text())
    if not m:
        raise SystemExit(
            "FAIL: PROBE.TXT has no game_mode line. That field IS this "
            "gate — whose turn it is has no other stable trace.")
    return int(m.group(1), 16), int(m.group(2), 16)


def main() -> int:
    ok = True
    for mode, l2, lives, want_player, why in (
            ("0", "", "3", 0, "1 Player: the turn cannot change"),
            ("1", "", "3", 1, "2 Players: the life loss hands over"),
            ("1", "BATTY_REPLAY_LIVES_2UP=0", "3", 0,
             "2 Players but player 2 is out: the guard holds the turn")):
        got = probe(mode, l2, lives)
        if got is None:
            print(f"  mode {mode} {l2}: NO PROBE.TXT [FAIL]")
            ok = False
            continue
        got_mode, got_player = got
        good = (got_mode == int(mode) and got_player == want_player)
        ok = ok and good
        print(f"  mode={mode} lives={lives} {l2 or '-':26s} "
              f"game_mode={got_mode} "
              f"active_player={got_player} "
              f"[{'PASS' if good else 'FAIL'}] "
              f"(expect player={want_player} — {why})")

    if ok:
        print("PASS two_player_turn: a life loss hands over in mode 1 and "
              "does not in mode 0")
        return 0
    print("FAIL two_player_turn")
    return 1


if __name__ == "__main__":
    sys.exit(main())
