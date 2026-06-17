#!/usr/bin/env python3
"""Generalized sprite attribute-parity invariant (the known-bugs #7 class,
for ALL moving sprites — not just the enemy).

The original draws every moving object with print_obj_to_buff ($B82C) —
sprite PIXELS only, never print_sprite_attrib — so a moving sprite leaves
each cell's ATTRIBUTE untouched and renders in ZX colour-clash with whatever
is underneath (bg over texture, the BRICK's attr over bricks). #7 was the
ENEMY violating this (force-recoloured to bg_attr); it's fixed and gated by
test-enemy-attr-parity. This gate locks the SAME rule for the other moving
sprites — falling bonus, enemy bomb, laser bullet — which already inherit the
cell attr (their renderers ignore their bg arg) but were never guarded.

Invariant (oracle-free, whole-field): with the primary ball stuck on the bat
(so nothing hits a brick and no brick attr legitimately changes), a sprite
seeded over the L3 brick band must leave attr_buff == bg_attr_buff for ALL
768 cells across several frames. Any difference = that sprite recoloured a
cell it flew/fell over = a #7-class regression -> FAIL.

    make test-sprite-attr-parity
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu, boot_until_gameplay

FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-test.img")
OUT = Path("build/test_sprite_attr_parity")
BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BOOT_WAIT = os.environ.get("BATTY_BOOT_WAIT", "8")

# Each sprite seeded over the L3 brick band (y 32..128). The ball is stuck on
# the bat (BATTY_REPLAY_BALL_STUCK=1) so it never hits a brick.
#   (name, extra-env, frames)
SPRITES = [
    ("bonus",  "BATTY_REPLAY_BONUS=0,120,48", (4, 10)),   # type 0 over bricks, falls
    ("bomb",   "BATTY_REPLAY_BOMB=140,40",    (4, 10)),   # enemy bomb, falls
    ("bullet", "BATTY_REPLAY_BULLET=100,110", (4, 8)),    # laser bullet, rises into bricks
]


def probe(extra_env: str, frame: int, label: str) -> dict:
    Path(FLOPPY).unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} BATTY_REPLAY_BALL_STUCK=1 "
        f"{extra_env} BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)

    def drive():
        subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                        "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                        "--out", str(OUT / "tl")], stdout=subprocess.DEVNULL)
    return boot_until_gameplay(FLOPPY, drive, label=label)


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    fails = 0
    for name, extra_env, frames in SPRITES:
        for frame in frames:
            p = probe(extra_env, frame, f"{name} f{frame}")
            ab = bytes.fromhex(p.get("attr_buff", ""))
            bg = bytes.fromhex(p.get("bg_attr_buff", ""))
            if len(ab) != 768 or len(bg) != 768:
                print(f"  {name} f{frame}: missing attr probe [FAIL]")
                fails += 1
                continue
            diffs = [(i, ab[i], bg[i]) for i in range(768) if ab[i] != bg[i]]
            tag = "PASS" if not diffs else "FAIL"
            print(f"  {name} f{frame}: attr!=bg cells={len(diffs)} [{tag}]")
            for (i, a, b) in diffs[:6]:
                print(f"      cell r{i // 32} c{i % 32}: attr=0x{a:02X} bg=0x{b:02X}")
            if diffs:
                fails += 1

    print()
    if fails:
        print(f"FAIL sprite_attr_parity: {fails} case(s) recoloured a cell "
              f"(moving sprites must leave attrs untouched)")
        return 1
    print("PASS sprite_attr_parity: falling bonus / bomb / bullet leave every "
          "cell attr unchanged (ZX colour-clash, matches the original)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
