#!/usr/bin/env python3
"""Guard the game-over sequence's shape in source.

Nothing else covers it. `make test`'s four-state cycle stops at LEVEL,
and scripts/exercise_gameloop.py drives the real thing but is a manual
tool — it loses three lives on three 9-second wall-clock waits, which is
the same fragility that makes test-bat-redraw-window flaky
(notes/testing.md). A gate built that way would flake too.

So this checks the ordering facts that a visual capture could not tell
apart anyway, all ported from LBC10_6:

  - the high score is captured BEFORE the screen is drawn, or the
    player's final score never reaches the name-entry save
  - the save happens AFTER name entry, so the file gets the score and
    the initials together
  - the hold is ~3.6 s and any key cuts it short. The DURATION comes
    from the original's pause_long B=$0C = 12 * 0.3 s at 18.2 Hz = 65
    BIOS ticks; the port counts it in PIT frames (178 at ~50 Hz) because
    bios_ticks() does not advance during gameplay — known-bugs.md #15,
    where counting it in BIOS ticks made this loop infinite
  - no sound is played; the original's pause_clear_screen_attrib just
    drains the queue while the screen clears

This is the part that can be checked without an emulator; the screen
itself is now covered by `test-game-over-visual`, which reaches it with
BATTY_REPLAY_LIVES=1 + BATTY_HIDE_BALL and holds it with
BATTY_HOLD_GAME_OVER.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "main.cpp"


def body_of(src: str, signature: str) -> str:
    start = src.index(signature)
    depth = 0
    i = src.index("{", start)
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i:j + 1]
    raise SystemExit(f"FAIL: could not find the end of {signature}")


def main() -> int:
    src = SRC.read_text()

    if "static void play_game_over(void)" not in src:
        raise SystemExit("FAIL: play_game_over is gone; update this gate")
    body = body_of(src, "static void play_game_over(void)")
    # Comments are stripped BEFORE compacting. They must be: this gate
    # asserts on the ABSENCE of things ("no sound_queue", "not back on
    # bios_ticks"), and a comment explaining why something is absent
    # would otherwise trip the check that guards it.
    code = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
    code = re.sub(r"//[^\n]*", " ", code)
    compact = "".join(code.split())

    if "player.lives==0){game_overs_reached++;play_game_over();" \
            not in "".join(src.split()):
        raise SystemExit("FAIL: game over is no longer entered from lives == 0")

    order = [
        ("player.score>high_score", "capture the high score"),
        ("render_game_over();", "draw the screen"),
        ("input_new_record_name();", "ask for initials"),
        ("high_score_save(", "save"),
    ]
    # Locate each step independently, THEN check the order. Searching
    # forward from the previous hit would report a swapped pair as a
    # missing call and send the reader after the wrong thing.
    where = []
    for needle, what in order:
        found = compact.find(needle.replace(" ", ""))
        if found < 0:
            raise SystemExit(f"FAIL: play_game_over no longer does: {what}")
        where.append((found, what))
    if where != sorted(where):
        actual = " -> ".join(w for _, w in sorted(where))
        wanted = " -> ".join(w for _, w in order)
        raise SystemExit(f"FAIL: play_game_over runs {actual}; expected {wanted}")
    print("PASS game_over_order: capture high score -> draw -> initials -> save")

    if "178UL" not in compact:
        raise SystemExit("FAIL: GAME OVER hold is no longer 178 PIT frames "
                         "(pause_long B=$0C = 12 * 0.3 s at 18.2 Hz)")
    if "if(kbhit()){getch();break;}" not in compact:
        raise SystemExit("FAIL: GAME OVER hold can no longer be cut short by a key")
    if "bios_ticks" in compact:
        raise SystemExit(
            "FAIL: play_game_over is back on bios_ticks, which does not "
            "advance during gameplay (known-bugs.md #15) — the hold would "
            "never expire and the screen would sit there until a keypress")
    print("PASS game_over_hold: ~178 PIT frames (= 3.6 s), any key cuts it short")

    if "sound_queue(" in compact:
        raise SystemExit("FAIL: play_game_over queues a sound; LBC10_6 plays none")
    if "sound_stop_all();" not in compact:
        raise SystemExit("FAIL: play_game_over no longer silences the queue first")
    print("PASS game_over_silent: no sound queued, queue stopped up front")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
