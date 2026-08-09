#!/usr/bin/env python3
"""check_margins CLAMPS an alien; it does not bounce it.

    check_top_margin    y < $08              -> y = $08
    check_left_margin   x < $08              -> x = $08
    check_right_margin  (u8)(w + x) >= $F9   -> x = $F8 - w

Three clamps, no direction change, no re-aim. `LAA7D` turns one step
toward the held target and re-picks only on ARRIVAL, so nothing in the
original steers an alien off a wall: it presses against the clamp, its
dir eventually reaches its target, the arrival re-pick sends it
somewhere random, and it leaves.

Until 2026-08-09 the port reflected the direction off each edge and
called an invented `enemy_target_away_from_margins`. Replacing both with
the literal clamps left ALL 76 GATES GREEN — no gate had ever watched an
alien reach a margin. That is why this one exists.

The scenario seeds an alien at x=12, y=20 (above the brick band, so
LAFFC cannot interfere) heading left with target == dir, and watches it
arrive at the wall and stay there.

PHASE. Steering is gated on `pit_frame_counter & 3`, whose phase depends
on how long boot took (known-bugs #17), so `target` is never assertable
and `dir` is only assertable RELATIVE to how many turns have run. Each
non-arrival turn moves dir by exactly one 6-bit step, so

    shortest_arc(dir, $20) <= turns - arrival

is exact, phase-independent, and still catches a reflection: the old
code flipped dir to $00 in a single frame, an arc of 32.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-margin.img")

# Alien at x=$0C (12), y=$14 (20), dir=$20 (left), target=$20, w=$18.
ENEMY = "0905 0C47 1464 2001 030F DD74 180C A41C 030F 3070 2000".replace(" ", "")
BAT   = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BALL  = "02006C004E001F03020CEEF008076C4E020C0000008C"

START_DIR = 0x20
CLAMP_X = 8
EXP_Y = 20

# (frame, expected x). Speed 1 is 2px/frame here: 12 -> 10 -> 8, and
# then the clamp holds it. Frames past the clamp are the real assertion —
# under the old reflect-and-re-aim, dir flipped to $00 and x climbed away
# from the wall instead of resting on it.
CASES = [(2, 10), (4, CLAMP_X), (6, CLAMP_X), (8, CLAMP_X)]


def shortest_arc(a: int, b: int) -> int:
    d = (a - b) & 0x3F
    return min(d, 0x40 - d)


def probe(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_margin.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_LEVEL=3 BATTY_START_LEVEL=1 "
        f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_PROBE=1 "
        f"BATTY_REPLAY_RANDOM=3793 BATTY_REPLAY_RANDOM_SEED=962A "
        f"BATTY_REPLAY_BAT_OBJECT={BAT} BATTY_REPLAY_BALL_STUCK=0 "
        f"BATTY_REPLAY_BALL_OBJECT={BALL} "
        f"BATTY_REPLAY_ENEMY_OBJECT={ENEMY} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_margin"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    text = out.read_text()
    m = re.search(r"object_enemy=([0-9A-Fa-f]+)", text)
    r = re.search(r"enemy_repicks=arrival(\d+)_turns(\d+)", text)
    if not m or not r:
        return None
    return bytes.fromhex(m.group(1)), int(r.group(1)), int(r.group(2))


def main() -> int:
    ok = True
    for frame, exp_x in CASES:
        got = probe(frame)
        if got is None:
            print(f"  frame {frame}: NO enemy in PROBE.TXT [FAIL]")
            ok = False
            continue
        b, arrival, turns = got
        x, y, d = b[2], b[4], b[6]
        steps = turns - arrival           # turns that actually moved dir
        arc = shortest_arc(d, START_DIR)
        good = (x == exp_x and y == EXP_Y and arc <= steps)
        ok = ok and good
        why = "" if arc <= steps else (
            f" — dir moved {arc} steps but only {steps} turns could have "
            f"moved it; a reflection off the wall is an arc of 32")
        print(f"  frame {frame}: x={x} y={y} dir=0x{d:02X} "
              f"arrival={arrival} turns={turns} arc={arc} "
              f"[{'PASS' if good else 'FAIL'}] (expect x={exp_x} "
              f"y={EXP_Y} arc<={steps}){why}")

    if ok:
        print("PASS enemy_margin_clamp: the alien rests on the left clamp "
              "with its direction untouched")
        return 0
    print("FAIL enemy_margin_clamp")
    return 1


if __name__ == "__main__":
    sys.exit(main())
