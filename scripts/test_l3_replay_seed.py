#!/usr/bin/env python3
"""Regression checks for the deterministic L3 brick replay seed.

The L3 replay is only useful if both runners really start from the same
class of state. This catches cheap-to-miss drift in the JSON setup and
Makefile env before the expensive emulator gate runs.
"""
import json
import re
from pathlib import Path


REPLAY = Path("replays/l3-brick-flash.json")
MAKEFILE = Path("Makefile")

BALL_OBJECT_HEX = "02006C004E001F03020CEEF008076C4E020C0000008C"


def hex_bytes(values: list[str]) -> str:
    return "".join(f"{int(v, 0):02X}" for v in values)


def main() -> int:
    spec = json.loads(REPLAY.read_text())
    setup = spec["original"]["setup"]
    writes = [step for step in setup if step.get("op") == "write_memory"]
    ball_writes = [step for step in writes if int(step.get("address", "0"), 0) == 0x9AD0]
    if len(ball_writes) != 1:
        raise SystemExit(f"FAIL: expected one object_ball_1 seed write at $9AD0, found {len(ball_writes)}")
    ball = bytes(int(v, 0) for v in ball_writes[0]["bytes"])
    if len(ball) != 22:
        raise SystemExit(f"FAIL: object_ball_1 seed must be 22 bytes, got {len(ball)}")
    if ball.hex().upper() != BALL_OBJECT_HEX:
        raise SystemExit("FAIL: JSON object_ball_1 seed drifted from Makefile guard value")
    if ball[0x06] != 0x1F:
        raise SystemExit(f"FAIL: object_ball_1+$06 direction must be $1F, got ${ball[0x06]:02X}")
    if ball[0x14] != 0x00:
        raise SystemExit(f"FAIL: object_ball_1+$14 launch/stuck counter must be $00, got ${ball[0x14]:02X}")
    print("PASS l3_replay_seed_ball: original seed is in-flight with direction $1F")

    probes = spec["state_probe"]["original"]
    level_rows = [p for p in probes if p.get("name") == "current_level_copy"]
    if len(level_rows) != 1:
        raise SystemExit(f"FAIL: expected one original current_level_copy probe, found {len(level_rows)}")
    level_addr = int(level_rows[0]["address"], 0)
    if level_addr != 0x6E43:
        raise SystemExit(f"FAIL: original current_level_copy probe must read active L3 $6E43, got ${level_addr:04X}")
    print("PASS l3_replay_seed_probe: original probe reads active L3 buffer $6E43")

    comparison = spec["comparison"]
    required = set(comparison.get("required_probe_rows", []))
    if "bricks_quantity" not in required:
        raise SystemExit("FAIL: bricks_quantity must be a required two-runner probe row")
    assertions = comparison.get("probe_assertions", [])
    has_original_destroyed = any(
        a.get("side") == "original"
        and a.get("name") == "current_level_copy"
        and a.get("op") == "contains"
        and str(a.get("value")).upper() == "93"
        for a in assertions
    )
    if not has_original_destroyed:
        raise SystemExit("FAIL: original active level must assert a destroyed $13 marker ($93)")
    print("PASS l3_replay_seed_gate: two-runner brick state is fail-gated")

    makefile = MAKEFILE.read_text()
    env_values = re.findall(r"BATTY_REPLAY_BALL_OBJECT=([0-9A-F]+)", makefile)
    if not env_values:
        raise SystemExit("FAIL: Makefile does not set BATTY_REPLAY_BALL_OBJECT for L3 replay")
    if any(v != BALL_OBJECT_HEX for v in env_values[:2]):
        raise SystemExit(f"FAIL: Makefile L3 replay ball object does not match JSON seed: {env_values[:2]}")
    if "BATTY_REPLAY_BALL_STUCK=0" not in makefile:
        raise SystemExit("FAIL: Makefile L3 replay must force BATTY_REPLAY_BALL_STUCK=0")
    if "BATTY_REPLAY_BALL_VEL=0,-3" not in makefile:
        raise SystemExit("FAIL: Makefile L3 replay must force BATTY_REPLAY_BALL_VEL=0,-3")
    print("PASS l3_replay_seed_makefile: Makefile env matches JSON seed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
