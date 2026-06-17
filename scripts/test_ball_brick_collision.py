#!/usr/bin/env python3
"""Collision-invariant sweep: a ball aimed into a SOLID brick must never
pass through it unhit (known-bugs #6 "ball teleported through red bricks").

This is a CLASS of regression test, not a single hand-tuned case. For each
level it boots once to read the initial brick grid (current_level_copy),
picks solid target bricks, then for each (target x approach x speed) seeds
the primary ball one step away aimed into the brick, runs N frames, and
asserts the *invariant*:

    a ball aimed into a still-solid brick must, within N frames, EITHER
    change that brick's state (destroyed / half-hit / brick count drops)
    OR reverse direction (bounce). If it does neither AND has advanced
    THROUGH the brick (crossed its far edge while still overlapping its
    column/row), it tunneled -> FAIL.

Geometry (matches brick_collision / laffc_collision in src/main.c):
  grid index = row*15 + col, row 0..11, col 0..14
  cell solid  = (v & 0x80) == 0      (0x80 = destroyed, 0xC0 = empty)
  brick_top_y = 32 + row*8 ; brick is 8 px tall, 16 px wide
  brick_left_x = 8  + col*16
Direction encoding (dir_to_dxdy / dir_sin_tbl, idx = dir & 0x0F):
  0x30 up   0x28 up-left   0x38 up-right
  0x10 down 0x18 down-left 0x08 down-right

No ZEsarUX oracle needed -- the invariant is intrinsic to the rules.

    make test-ball-no-tunnel            # quick subset (parity-check)
    make test-ball-no-tunnel FULL=1     # all 15 levels, diagonals (parity-check-full)
"""
from __future__ import annotations

import argparse
import math
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu

FLOPPY = Path("build/batty-test.img")
OUT = Path("build/test_ball_no_tunnel")

TAIL = "020CEEF008076C4E020C0000008C"
BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"

# approach name -> (vertical sense, direction byte). "up" picks the lowest
# solid brick in a column (clear path below); "down" the highest (clear
# path above). Diagonals exercise the LAFFC straddle / corner-case logic.
APPROACHES = {
    "up":     ("up",   0x30),
    "up-l":   ("up",   0x28),
    "up-r":   ("up",   0x38),
    "down":   ("down", 0x10),
    "down-l": ("down", 0x18),
    "down-r": ("down", 0x08),
}


def ball_seed(x: int, y: int, direction: int, speed: int) -> str:
    return f"0200{x:02X}00{y:02X}00{direction:02X}{speed:02X}{TAIL}"


def build_floppy(env: str) -> None:
    FLOPPY.unlink(missing_ok=True)
    subprocess.run(f"{env} make {FLOPPY}", shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)


def parse_probe(text: str) -> dict:
    out = {}
    for line in text.splitlines():
        if "=" in line and not line.startswith("#"):
            k, v = line.split("=", 1)
            out[k.strip()] = v.strip()
    return out


def boot_and_read(sleep_extra: float) -> dict:
    subprocess.run(["mdel", "-i", str(FLOPPY), "::PROBE.TXT"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    run_qemu(FLOPPY, ["SLEEP 9.0", "sendkey ret", f"SLEEP {1.0 + sleep_extra}"],
             OUT / "qemu.log")
    try:
        raw = subprocess.check_output(["mtype", "-i", str(FLOPPY), "::PROBE.TXT"],
                                      stderr=subprocess.STDOUT).decode("ascii", "replace")
    except subprocess.CalledProcessError:
        return {}
    return parse_probe(raw)


def is_solid(v: int) -> bool:
    return (v & 0x80) == 0


def read_initial_grid(level: int) -> tuple[bytes, int]:
    build_floppy(f"BATTY_LEVEL={level} BATTY_START_LEVEL=1 "
                 f"BATTY_REPLAY_PROBE=1 BATTY_FRAME_PROBE=1")
    p = boot_and_read(0.5)
    g = bytes.fromhex(p.get("current_level_copy", ""))
    if len(g) != 180:
        raise SystemExit(f"FAIL L{level}: bad initial grid ({len(g)} cells)")
    return g, int(p.get("bricks_quantity", "0"), 16)


def pick_targets(grid: bytes, vsense: str, max_cols: int) -> list[tuple[int, int]]:
    targets = []
    for c in range(15):
        col_solid = [r for r in range(12) if is_solid(grid[r * 15 + c])]
        if not col_solid:
            continue
        if vsense == "up":
            r = max(col_solid)
            if r >= 11:
                continue
        else:
            r = min(col_solid)
        targets.append((r, c))
    if len(targets) > max_cols:
        step = len(targets) / max_cols
        targets = [targets[int(i * step)] for i in range(max_cols)]
    return targets


def run_case(level, row, col, vsense, direction, speed) -> dict | None:
    brick_top = 32 + row * 8
    brick_bot = brick_top + 8
    brick_left = 8 + col * 16
    x = brick_left + 4
    if vsense == "up":
        y = brick_bot + 1
        clear_px = (y + 6) - (brick_top - 1)
    else:
        y = brick_top - 7 - 1
        clear_px = brick_bot - y
    if y < 8 or y + 6 >= 128:
        return None
    # diagonal dy magnitude is ~0.7x of straight; pad frames so a FREE ball
    # would clear the brick well before the probe.
    frames = math.ceil(clear_px / max(speed * 0.6, 1)) + 4

    env = (f"BATTY_LEVEL={level} BATTY_START_LEVEL=1 BATTY_REPLAY_PROBE=1 "
           f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_RANDOM=8E49 "
           f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} "
           f"BATTY_REPLAY_BALL_OBJECT={ball_seed(x, y, direction, speed)} "
           f"BATTY_REPLAY_BALL_STUCK=0 BATTY_FRAME_PROBE={frames}")
    build_floppy(env)
    p = boot_and_read(frames * 0.05)
    g = bytes.fromhex(p.get("current_level_copy", ""))
    b = bytes.fromhex(p.get("object_ball_1", ""))
    if len(g) != 180 or len(b) != 22:
        return {"err": "bad probe"}
    return {"row": row, "col": col, "vsense": vsense, "direction": direction,
            "speed": speed, "frames": frames, "seed_y": y, "brick_top": brick_top,
            "brick_bot": brick_bot, "brick_left": brick_left,
            "final_x": b[2], "final_y": b[4], "final_dir": b[6],
            "cell_now": g[row * 15 + col],
            "bricks_now": int(p.get("bricks_quantity", "0"), 16)}


def evaluate(case: dict, bricks0: int) -> tuple[bool, str]:
    if case is None:
        return True, "skipped (out of band)"
    if "final_y" not in case:
        return False, case.get("err", "no result")
    destroyed = (case["cell_now"] & 0x80) != 0
    count_dropped = case["bricks_now"] < bricks0
    bounced = case["final_dir"] != case["direction"]
    if destroyed or count_dropped:
        return True, "brick hit (state changed)"
    if bounced:
        return True, f"bounced (dir {case['direction']:#04x}->{case['final_dir']:#04x})"
    # No interaction registered. Did the ball go THROUGH the brick (crossed
    # its far edge while still overlapping the brick's column)? Going AROUND
    # (drifted out of the column) is legitimate, not a tunnel.
    fx = case["final_x"]
    x_overlap = (fx + 7 >= case["brick_left"]) and (fx <= case["brick_left"] + 15)
    if case["vsense"] == "up":
        crossed = case["final_y"] + 6 < case["brick_top"]
    else:
        crossed = case["final_y"] > case["brick_bot"]
    if crossed and x_overlap:
        return False, (f"TUNNELED through cell 0x{case['cell_now']:02X}: "
                       f"dir {case['final_dir']:#04x} unchanged, ball at "
                       f"x={fx} y={case['final_y']} past brick "
                       f"top={case['brick_top']} bot={case['brick_bot']}")
    return True, "no interaction (ball went around / didn't reach brick)"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--levels", default="1,2,3")
    ap.add_argument("--speeds", default="6")
    ap.add_argument("--approaches", default="up,down")
    ap.add_argument("--max-cols", type=int, default=3)
    ap.add_argument("--full", action="store_true")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)

    if args.full:
        levels = list(range(1, 16))
        speeds = [2, 4, 6]
        approaches = list(APPROACHES.keys())
        max_cols = 3
    else:
        levels = [int(x) for x in args.levels.split(",")]
        speeds = [int(x) for x in args.speeds.split(",")]
        approaches = args.approaches.split(",")
        max_cols = args.max_cols

    fails, total = [], 0
    for level in levels:
        grid0, bricks0 = read_initial_grid(level)
        for ap_name in approaches:
            vsense, direction = APPROACHES[ap_name]
            for (row, col) in pick_targets(grid0, vsense, max_cols):
                for speed in speeds:
                    case = run_case(level, row, col, vsense, direction, speed)
                    if case is None:
                        continue
                    total += 1
                    ok, reason = evaluate(case, bricks0)
                    print(f"  L{level:2d} r{row} c{col:2d} {ap_name:6s} spd{speed} "
                          f"f{case.get('frames','?'):>2}: [{'PASS' if ok else 'FAIL'}] {reason}")
                    if not ok:
                        fails.append((level, row, col, ap_name, speed, reason))

    print()
    if fails:
        print(f"FAIL ball_no_tunnel: {len(fails)}/{total} cases tunneled/erred")
        for (lv, r, c, a, s, why) in fails:
            print(f"    L{lv} r{r} c{c} {a} spd{s}: {why}")
        return 1
    print(f"PASS ball_no_tunnel: {total} cases, no ball passed through a solid brick")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
