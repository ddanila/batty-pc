#!/usr/bin/env python3
"""Regression check for the rocket bonus no-ball state.

The original main loop checks object_rocket before balls_quantity
(LBAED -> LBAED_6), so a caught rocket can hide all balls while the
level-clear sequence runs without entering LBC10's bat-death path.
"""
from pathlib import Path


SRC = Path("src/main.c")


def main() -> int:
    src = SRC.read_text()
    compact = "".join(src.split())
    guard = "if(!rocket_active&&!BALL_VISIBLE&&!ball2_active&&!ball3_active)"
    if guard not in compact:
        raise SystemExit(f"FAIL: missing rocket-safe no-ball death guard: {guard}")
    idx = compact.find(guard)
    if "play_bat_explosion(current_level_idx_var);" not in compact[idx:idx + 220]:
        raise SystemExit("FAIL: rocket-safe no-ball guard does not protect bat-explosion path")
    if "Mirror LBAED's ordering" not in src or "before balls_quantity" not in src:
        raise SystemExit("FAIL: rocket-safe no-ball guard lacks original-code breadcrumb")
    print("PASS rocket_bonus_no_bat_death: no-ball death guard excludes rocket_active")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
