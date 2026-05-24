#!/usr/bin/env python3
"""Fail-gate the deterministic L3 mid-game brick replay.

The replay starts L3 with a seeded in-flight ball near destructible
bricks. This test reads the DOS port's post-run PROBE.TXT extraction and
asserts that the run actually exercised brick destruction rather than
silently capturing the level-entry state.
"""
from pathlib import Path


PROBE = Path("build/replay/l3-brick-flash/port/state_probe.txt")


def read_probe(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(errors="replace").splitlines():
        if line.startswith("#") or ":" not in line:
            continue
        key, value = line.split(":", 1)
        values[key.strip()] = value.strip()
    return values


def main() -> int:
    if not PROBE.exists():
        raise SystemExit(f"FAIL: missing replay probe {PROBE}; run make replay-l3-brick-flash first")
    probe = read_probe(PROBE)
    bricks = int(probe.get("bricks_quantity", "FF"), 16)
    score = int(probe.get("score", "0"), 10)
    rng = probe.get("random_number")
    level = probe.get("current_level_copy", "")
    bonus = probe.get("bonus_state", "")
    ball = probe.get("object_ball_1", "")

    if bricks >= 0x1A:
        raise SystemExit(f"FAIL: replay did not destroy any bricks; bricks_quantity={bricks:02X}")
    if score <= 0:
        raise SystemExit(f"FAIL: replay did not award brick score; score={score:06d}")
    if rng == "8E49":
        raise SystemExit("FAIL: replay did not advance RNG from seeded value 8E49")
    if "93" not in level:
        raise SystemExit("FAIL: replay level copy does not contain a destroyed $13 brick marker")
    if len(bonus) != 14:
        raise SystemExit(f"FAIL: malformed bonus_state probe: {bonus!r}")
    if len(ball) != 44:
        raise SystemExit(f"FAIL: malformed object_ball_1 probe: {ball!r}")
    if ball.startswith("02008400A600"):
        raise SystemExit("FAIL: replay ball respawned on the bat instead of staying in descriptor-motion play")

    print(f"PASS midgame_brick_replay: bricks={bricks:02X}, score={score:06d}, rng={rng}, ball={ball[:12]}, bonus_state={bonus}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
