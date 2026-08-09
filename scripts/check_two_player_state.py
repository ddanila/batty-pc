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
  - the round and level counters live in `PlayerState`, because the
    original keeps them in the eight-byte block `players_swap`
    exchanges — each player resumes their own round and level.

### The 1UP slot really does mean player 1

`players_swap` swaps the two score blocks in memory rather than
indexing, which looks at first like the 1UP position would show whoever
is currently playing. It does not: the block starts with the SCREEN
ADDRESS of its own slot ($0F00 for 1UP, $0FD0 for 2UP) and the swap
covers `B=$0A` — 2 address + 2 rudiment + 6 digits. The address travels
with the digits, so each player keeps their own position on screen.
Checked because this gate asserts it.
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

    # The round/level counters belong to the player, not the machine.
    for field in ("level_number", "round_number"):
        if f"unsigned char {field};" not in "".join(
                l for l in struct.split("\n")):
            raise SystemExit(
                f"FAIL: PlayerState has no {field}. The original's "
                f"per-player block at lives_1up carries lives, "
                f"briks_quantity, current_level_number, round_number, the "
                f"score and ctrl_type, and players_swap exchanges all eight "
                f"bytes — so each player resumes their OWN round and level.")
    for macro, target in (("round_number", "players[active_player].round_number"),
                          ("current_level_idx_var",
                           "players[active_player].level_number")):
        if f"#define {macro}" not in code:
            raise SystemExit(f"FAIL: {macro} is no longer the active "
                             f"player's; it went back to being one global "
                             f"shared by both players.")
        line = [l for l in code.split("\n") if l.startswith(f"#define {macro}")][0]
        if "".join(target.split()) not in "".join(line.split()):
            raise SystemExit(f"FAIL: {macro} maps to `{line}`, expected "
                             f"`{target}`")
    print("PASS per_player_progress: round and level are the active "
          "player's")

    # game_mode is 0-based; selected_mode is 1..3. One conversion site.
    conv = body_of(code, "static unsigned char game_mode_from_selection(")
    if "selection - 1" not in conv:
        raise SystemExit(
            "FAIL: game_mode_from_selection no longer subtracts one. The "
            "original's game_mode is 0-based (0 = 1 Player, 1 = 2 Players, "
            "2 = Double Play) while the menu's selected_mode comes from "
            "`k - '0'` and is 1..3. notes/menu.md.")
    # `game_mode == 1` contains the substring "game_mode =", so an
    # assignment test written that way flags every comparison. It did,
    # the first time this ran against the turn-change code.
    ASSIGN = re.compile(r"\bgame_mode\s*=(?!=)")
    users = [l for l in code.split("\n")
             if ASSIGN.search(l) and "game_mode_from_selection" not in l
             and "static unsigned char game_mode" not in l]
    for line in users:
        if "BATTY_GAME_MODE" in code.split(line)[0][-400:]:
            continue        # the env knob, which takes the 0-based value
        raise SystemExit(
            f"FAIL: game_mode is assigned outside "
            f"game_mode_from_selection:\n  {line.strip()}\n"
            f"Every menu-side assignment must go through the conversion, "
            f"or a 1..3 value reaches code that expects 0..2.")
    print("PASS game_mode_zero_based: one conversion site, and it "
          "subtracts one")

    # The banner digit is state, not a literal.
    try:
        banner = body_of(code, "static void draw_round_banner(int round_num) {")
    except ValueError:
        raise SystemExit("FAIL: draw_round_banner is gone; if the banner "
                         "moved, point this gate at it")
    if "active_player + 1" not in banner:
        raise SystemExit(
            "FAIL: the round banner no longer prints active_player + 1. "
            "The original is `LD A,(player_number) / INC A / "
            "LD (txt_player_x+11),A`; a hardcoded 1 is the stub this "
            "replaced, and it renders identically today.")
    print("PASS banner_player_digit: the banner prints active_player + 1")

    # Same for the GAME OVER screen's second line. test-game-over-visual
    # covers that the LINE is drawn, but not the digit: active_player is
    # 0 in its scenario, so `active_player + 1` and a literal 1 render
    # the same glyph. Mutating the digit to 0x01 SURVIVED that gate.
    go = body_of(code, "static void render_game_over(void) {")
    if "pl[8] = (unsigned char)(active_player + 1);" not in go:
        raise SystemExit(
            "FAIL: the GAME OVER screen's PLAYER line no longer prints "
            "active_player + 1. The original patches txt_player_0+$0C with "
            "`LD A,(player_number) / INC A`; a literal renders identically "
            "in a 1-player capture, so no pixel gate can hold this.")
    print("PASS game_over_player_digit: the GAME OVER line prints "
          "active_player + 1")

    # Scoring has ONE owner. orig add_points_to_score ($018D) decides,
    # in Double Play, WHICH player a score goes to from the side the
    # event happened on. A second `+=` site would silently bypass that —
    # and the duplicate-guard lesson applies here too: two places that
    # decide the same thing means one of them is untested.
    helper = body_of(code, "static void add_points_to_score(unsigned long pts, int side_x) {")
    strays = []
    for n, line in enumerate(code.split("\n"), 1):
        if re.search(r"\.score\s*\+=", line) and line not in helper:
            strays.append(f"{n}: {line.strip()}")
    if strays:
        raise SystemExit(
            "FAIL: score is added outside add_points_to_score:\n  "
            + "\n  ".join(strays)
            + "\nEvery scoring site must route through it, or Double Play "
              "credits the wrong player. notes/double-play.md.")
    print("PASS score_single_owner: every score add goes through "
          "add_points_to_score")

    flat_helper = "".join(helper.split())
    if "game_mode==2" not in flat_helper or "&0x80" not in flat_helper:
        raise SystemExit(
            "FAIL: add_points_to_score no longer decides by game_mode $02 "
            "and the side's top bit. The original is `CP $02 / JR NZ` then "
            "`LD A,(need_change_player) / AND A / JR Z`, and "
            "need_change_player is always an `AND $80` of some object's "
            "coordinate.")
    print("PASS score_side_rule: it keys on game_mode $02 and the top bit")

    # The BRICK sites take the ball's owner, not a position and not
    # SIDE_ACTIVE. handling_ball's flag is (IX+$12) & $80, a persistent
    # owner bit that nothing in flight changes — so brick points follow
    # the BALL, wherever the brick is. Both mutations below SURVIVED
    # until this check existed.
    brick_owner = len(re.findall(
        r"add_points_to_score\(pts, ball_owner_side \? 0x80 : 0\)", code))
    if brick_owner < 2:
        raise SystemExit(
            f"FAIL: only {brick_owner} brick-scoring site(s) use "
            f"ball_owner_side; expected the two that handling_ball covers "
            f"(the LAFFC path and the sweep path). A brick score keyed on "
            f"anything else — a coordinate, or SIDE_ACTIVE — credits the "
            f"wrong player in Double Play. notes/double-play.md.")
    print(f"PASS brick_score_owner: {brick_owner} brick sites take the "
          f"ball's owner")

    # The owner comes from an ALTERNATING start side, orig
    # `LD A,(ball_x_coord+$01) / XOR $88 / LD (ball_x_coord+$01),A`.
    # Pinned to a toggle: assigning a constant leaves every ball owned by
    # player 1 and is invisible to everything else.
    if "ball_start_right = (unsigned char)(!ball_start_right);" not in code:
        raise SystemExit(
            "FAIL: the ball's start side no longer alternates. The original "
            "flips it at every all_var_init with `XOR $88` on the "
            "self-modified start x ($48 <-> $C0); a fixed value leaves "
            "every ball owned by the same player.")
    print("PASS ball_start_alternates: the start side toggles each entry")

    # The rocket-clear tally is the odd one out: it splits the surviving
    # bricks EVENLY rather than by side. orig zeroes the flag before the
    # sweep and XORs it after every award.
    tally = body_of(code, "static void play_rocket_award_tally(unsigned char level_idx) {")
    flat_tally = "".join(tally.split())
    if "left_brik_side=0" not in flat_tally:
        raise SystemExit(
            "FAIL: the leftover-brick tally does not start on 1UP. The "
            "original does `XOR A / LD (need_change_player),A` before the "
            "sweep.")
    if "left_brik_side=(unsignedchar)(!left_brik_side)" not in flat_tally:
        raise SystemExit(
            "FAIL: the leftover-brick tally no longer alternates. The "
            "original does `LD A,(need_change_player) / XOR $01 / "
            "LD (need_change_player),A` after EVERY award, so the "
            "surviving bricks split evenly between the players — it is "
            "not a side rule. notes/double-play.md.")
    print("PASS left_brik_even_split: the tally alternates from 1UP")

    # And with every site attributed, the sentinel is gone.
    if re.search(r"^#define SIDE_ACTIVE", code, re.M):
        raise SystemExit(
            "FAIL: SIDE_ACTIVE is back. It existed only for the two sites "
            "whose side could not be derived; both can now, and a sentinel "
            "with no callers is an invitation to add one.")
    print("PASS no_side_sentinel: every scoring site passes a real side")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
