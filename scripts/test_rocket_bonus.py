#!/usr/bin/env python3
"""Regression checks for the rocket / next-level bonus.

The original main loop checks object_rocket before balls_quantity
(LBAED -> LBAED_6), so a caught rocket can hide all balls while the
level-clear sequence runs without entering LBC10's bat-death path.

The original also mutates spr_bonus_rocket_1 in RAM: all_var_init keeps
the falling bonus height at $0C, and get_rocket patches it to $1B once
the rocket attaches to the bat. handling_rocket then writes rocket_y-6
into both bat objects so the bat flies up with the rocket.
"""
from pathlib import Path


SRC = Path("src/main.cpp")


def main() -> int:
    src = SRC.read_text()
    compact = "".join(src.split())
    # The extra-ball flags moved into BallState; the guard itself is
    # unchanged, and that is what this checks.
    guard = ("if(!rocket.active&&!rocket.clear_completed&&!dbg.suppress_no_ball_death"
             "&&!BALL_VISIBLE&&!ball.extra2_active&&!ball.extra3_active)")
    if guard not in compact:
        raise SystemExit(f"FAIL: missing rocket-safe no-ball death guard: {guard}")
    idx = compact.find(guard)
    if "lose_a_life();" not in compact[idx:idx + 220]:
        raise SystemExit("FAIL: rocket-safe no-ball guard does not protect the life-loss path")
    # The guard protects lose_a_life, so check that is still what explodes
    # the bat — otherwise this assertion could be satisfied by a helper
    # that no longer does.
    body = compact[compact.find("voidlose_a_life(void){"):]
    if "play_bat_explosion(current_level_idx_var);" not in body[:200]:
        raise SystemExit("FAIL: lose_a_life no longer plays the bat explosion")
    if "Mirror LBAED's ordering" not in src or "before balls_quantity" not in src:
        raise SystemExit("FAIL: rocket-safe no-ball guard lacks original-code breadcrumb")
    if "rocket.clear_completed = 1" not in src:
        raise SystemExit("FAIL: rocket exit does not bypass no-ball death until level clear")
    print("PASS rocket_bonus_no_bat_death: no-ball death guard excludes rocket_active")

    if "#define ROCKET_BONUS_H_PX 0x0C" not in src:
        raise SystemExit("FAIL: missing original falling-rocket height constant")
    if "set_rocket_bonus_sprite_height(ROCKET_BONUS_H_PX)" not in src:
        raise SystemExit("FAIL: falling rocket bonus height is not restored to $0C")
    if "set_rocket_bonus_sprite_height(ROCKET_H_PX)" not in src:
        raise SystemExit("FAIL: caught rocket sprite height is not patched to $1B")
    if "sprites_blob[SPR_BONUS_ROCKET_1 + 1] = height" not in src:
        raise SystemExit("FAIL: rocket height patch does not target spr_bonus_rocket_1+1")
    print("PASS rocket_bonus_sprite_height: falling/caught heights mirror original")

    if "BAT_Y = (unsigned char)(rocket.y - 6)" not in src:
        raise SystemExit("FAIL: handling_rocket does not attach bat Y to rocket_y - 6")
    if "objects[OBJ_BAT_2].y_coord = BAT_Y" not in src:
        raise SystemExit("FAIL: rocket flight does not update the second bat object Y")
    print("PASS rocket_bonus_attaches_bat: rocket flight lifts the bat")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
