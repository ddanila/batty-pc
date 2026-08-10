#!/usr/bin/env python3
"""Each of these state changes must have exactly ONE writer.

Several pieces of state in this port live in two places at once, and
writing one without the other is a bug no gate would catch:

  - an extra ball's liveness is a flag AND bit 7 of its sprite_set
  - the bat's active bonus lives in BOTH bat objects
  - the inner border line is two specific pixel columns

Each got a helper that writes both halves. But helpers do not
retroactively convert the code that predates them: `finish_cleared_level`
and `reset_level_state` were both found hand-rolling the extra-ball
clear AFTER `hide_extra_balls` existed, and `restore_inner_border_line`
re-implemented the border masks that `black_inner_border_pixels` owns.

Grepping for "did someone write this twice" is what this gate does, once
per invariant, in about a second. It is deliberately a COUNT: the
patterns are distinctive enough that a second occurrence is a
hand-rolled copy, and a count is stable against reformatting inside the
owner.

If a genuine second writer is ever needed, raise the count here and say
why — that is the point at which someone should think about it.
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "main.cpp"

# (needle, expected count, the function that should own it, why it matters)
OWNED = [
    ("objects[OBJ_BALL_2].sprite_set = 0x82;", 1, "hide_extra_balls",
     "an extra ball's liveness is the flag AND this bit; clearing one "
     "draws a ball that is never stepped, or steps one never drawn"),
    ("objects[OBJ_BALL_3].sprite_set = 0x82;", 1, "hide_extra_balls",
     "as above, for the third ball"),
    ("objects[bat].bonus_applied = code;", 1, "set_bat_bonus",
     "the CATCHING bat's bonus byte. This entry used to read "
     "`objects[OBJ_BAT_1].bonus_applied = code;` and justify itself with "
     "'the bat's bonus lives in both bat objects and they must not "
     "disagree' — which was wrong. The original keeps the two APART: "
     "LA67B_0 runs inside bonus_flag_swap for a bat-2 catch, so only the "
     "catching bat's byte moves, and effects that belong to the BALL "
     "(LA27E's big-ball test) read both bytes instead of relying on a "
     "mirror. Corrected 2026-08-10; see notes/double-play.md"),
    ("e->sprite_set = 0x0A;", 1, "blast_active_alien",
     "the alien-to-blast transition was once copied four times"),
    ("rocket.x = BAT_X + 4;", 1, "place_rocket_on_bat",
     "the real catch and the replay seed place the rocket identically"),
    ("scr_buff[y * 32 + 1] &= 0x7F;", 1, "black_inner_border_left",
     "this column going missing from one repaint path was known-bugs #11"),
    ("scr_buff[y * 32 + 30] &= 0xFE;", 1, "black_inner_border_right",
     "as above, the right-hand column"),
]


def main() -> int:
    src = SRC.read_text()
    compact = "".join(src.split())
    bad = []
    for needle, want, owner, why in OWNED:
        got = compact.count("".join(needle.split()))
        if got != want:
            bad.append((needle, want, got, owner, why))

    if bad:
        print(f"FAIL: {len(bad)} invariant(s) have the wrong number of writers\n")
        for needle, want, got, owner, why in bad:
            print(f"  {needle}")
            print(f"    expected {want} writer ({owner}), found {got}")
            print(f"    {why}")
            if got > want:
                print(f"    -> call {owner}() instead of writing it again")
            else:
                print(f"    -> did {owner}() change? update this gate with the reason")
            print()
        return 1

    print(f"PASS invariant_owners: {len(OWNED)} two-place state changes "
          f"still have one writer each")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
