#!/usr/bin/env python3
"""Per-player state is real state, and the HUD/cache both know it.

PLAN.md WS2 is 2-player alternating. This is its first stage: the score
and life counters are two, the HUD's 2UP slot shows the second one
instead of a literal zero, and the static-cache dirty test keys on both.
Nothing moves `active_player` off 0 yet, so behaviour is unchanged and
every QEMU gate sees exactly what it saw before.

### Why this is a source gate

The whole stage is invisible today. `players[1].score` is 0 in a
1-player game, and `draw_score_digits_original(0xC0, 0x15, 0)` drew the
same six zero glyphs. A screendump cannot tell the two apart, so no
pixel gate can hold this in place — and the natural regression is
somebody "simplifying" the 2UP slot back to a constant, or the dirty
test back to the active player.

What it pins:

  - `players[2]` exists and `high_score` is NOT inside it. One machine
    has one high score (`hi_score_in_game`); per-player it would make
    the middle HUD column change when the players swap.
  - the three HUD slots read `players[0]`, `high_score`, `players[1]` —
    not `player`, whose meaning depends on whose turn it is. The 1UP
    slot must show player 1 even while player 2 is playing.
  - the static cache's `drawn_score` is per-slot. A cache keyed on less
    than it paints is a bug waiting for a feature: with two players the
    2UP score changes while `player` does not, and the HUD would keep
    showing a stale number.
  - `new_game_reset` clears BOTH players, as `game_restart` does.
"""

from __future__ import annotations

import re
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


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def main() -> int:
    src = SRC.read_text()
    code = strip_comments(src)
    flat = "".join(code.split())

    if "PlayerStateplayers[2]" not in flat:
        raise SystemExit(
            "FAIL: there is no players[2]. The original keeps two of each "
            "counter (score_1up_in_game / score_2up_in_game, lives_1up / "
            "lives_2up) and game_restart zeroes both. PLAN.md WS2.")
    print("PASS two_player_state: players[2] exists")

    struct = body_of(code, "struct PlayerState {")
    if "high_score" in struct:
        raise SystemExit(
            "FAIL: high_score is back inside PlayerState. There is ONE high "
            "score for the machine (hi_score_in_game), shown in the middle "
            "HUD slot — per-player it would change when the players swap.")
    print("PASS high_score_not_per_player: PlayerState holds no high score")

    hud = body_of(code, "static void render_hud_to_buff(void) {")
    slots = re.findall(r"draw_score_digits_original\(\s*0x([0-9A-Fa-f]+)\s*,"
                       r"\s*0x[0-9A-Fa-f]+\s*,\s*([^)]+)\)", hud)
    want = {"10": "players[0].score", "68": "high_score",
            "C0": "players[1].score"}
    got = {x.upper(): "".join(v.split()) for x, v in slots}
    if got != want:
        raise SystemExit(
            f"FAIL: the HUD score slots read {got}, expected {want}.\n"
            f"The 1UP slot must show player 1 even while player 2 is "
            f"playing, so these read players[N] rather than `player`, whose "
            f"meaning depends on whose turn it is. A literal in the 2UP "
            f"slot is the stub this stage removed — and no screendump can "
            f"tell a literal 0 from a real 0.")
    print("PASS hud_slots: 1UP/HI/2UP read players[0], high_score, players[1]")

    if "unsigned long drawn_score[2];" not in code:
        raise SystemExit(
            "FAIL: the static cache's drawn_score is no longer per-slot. It "
            "must cover what is DRAWN: with two players the 2UP score "
            "changes while `player` does not, and the HUD would keep "
            "showing a stale number.")
    refresh = body_of(code, "static void refresh_static_background(unsigned char level_idx) {")
    for i in (0, 1):
        if f"players[{i}].score!=cache.drawn_score[{i}]" not in \
                "".join(refresh.split()):
            raise SystemExit(
                f"FAIL: the score-dirty test does not compare "
                f"players[{i}].score against cache.drawn_score[{i}].")
    print("PASS hud_cache_per_slot: the dirty test covers both score slots")

    reset = "".join(body_of(code, "static void new_game_reset(void) {").split())
    if "players[i].score=0" not in reset or "active_player=0" not in reset:
        raise SystemExit(
            "FAIL: new_game_reset no longer clears both players and returns "
            "to 1UP. game_restart zeroes BOTH scores and both life counts; "
            "resetting only the active player lets a 2-player game inherit "
            "the previous game's 2UP score.")
    print("PASS new_game_reset_both: both players cleared, turn back to 1UP")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
