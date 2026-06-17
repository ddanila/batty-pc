#!/usr/bin/env python3
"""No-tunnel coverage for the NON-primary ball collision paths (known-bugs
#6's secondary leads): step_extra_ball (multiball) and magnet_captured_move
(a ball captured + curved inside an ON magnet). Both call the SAME
`laffc_collision(); if (hit==0) brick_collision()` the primary ball uses
(the path the #6 edge-mask fix lives in), so this is a regression guard that
those shared-collision paths keep a ball out of solid bricks.

Invariant (oracle-free): at every probed frame, an ACTIVE ball's CENTRE must
not sit inside a solid brick cell. A correctly-bounced ball is snapped to the
cell EDGE (centre outside the brick); a ball whose centre is inside a still-
solid cell has penetrated = tunnel/stuck-in-brick -> FAIL.

  multiball: L1, BATTY_REPLAY_MULTIBALL seeds ball2@(96,150)/ball3@(160,150)
             moving in the primary's dir; aimed up they rise into the col-5
             and col-9 brick stacks. All three balls are checked.
  magnet:    L2, the test-magnet-ball seed (ball in the rows1-4/cols6-8
             pocket, MAGNET forced ON) — the captured ball curves against the
             bordering bricks.

    make test-ball-paths-no-tunnel
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu, boot_until_gameplay

FLOPPY = Path(os.environ.get("BATTY_TEST_FLOPPY", "build/batty-test.img"))
OUT = Path("build/test_ball_paths_no_tunnel")
TAIL = "020CEEF008076C4E020C0000008C"
BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BOOT_WAIT = os.environ.get("BATTY_BOOT_WAIT", "8")


def ball_seed(x, y, direction, speed):
    return f"0200{x:02X}00{y:02X}00{direction:02X}{speed:02X}{TAIL}"


def boot(env: str, frame: int, label: str) -> dict:
    FLOPPY.unlink(missing_ok=True)
    subprocess.run(f"{env} BATTY_FRAME_PROBE={frame} make {FLOPPY}", shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)

    def drive():
        subprocess.run(["mdel", "-i", str(FLOPPY), "::PROBE.TXT"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        run_qemu(FLOPPY, [f"SLEEP {BOOT_WAIT}", "sendkey ret",
                          f"SLEEP {1.0 + frame * 0.05}"], OUT / "qemu.log")
    return boot_until_gameplay(FLOPPY, drive, label=label)


def centre_in_solid(ball: bytes, grid: bytes):
    """Return (row,col,cellval) if the ACTIVE ball's centre is inside a solid
    brick cell, else None. Inactive (bit7 of sprite_set) -> None."""
    if ball[0] & 0x80:
        return None
    cx, cy = ball[2] + 4, ball[4] + 3
    if cy < 32 or cy >= 32 + 12 * 8 or cx < 8 or cx >= 8 + 15 * 16:
        return None
    row, col = (cy - 32) // 8, (cx - 8) // 16
    v = grid[row * 15 + col]
    return (row, col, v) if (v & 0x80) == 0 else None


def check(probe: dict, ball_keys, frame, label) -> list:
    grid = bytes.fromhex(probe.get("current_level_copy", ""))
    fails = []
    if len(grid) != 180:
        return [f"{label} f{frame}: bad grid"]
    for k in ball_keys:
        b = bytes.fromhex(probe.get(k, ""))
        if len(b) != 22:
            continue
        hit = centre_in_solid(b, grid)
        if hit:
            r, c, v = hit
            fails.append(f"{label} f{frame}: {k} centre inside SOLID cell "
                         f"r{r} c{c} (0x{v:02X}) at x={b[2]} y={b[4]} dir=0x{b[6]:02X}")
    return fails


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    fails = []

    # --- multiball: three balls rising into the L1 brick stacks ---
    mb_env = (
        "BATTY_LEVEL=1 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        "BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 BATTY_REPLAY_MULTIBALL=1 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} BATTY_REPLAY_BALL_STUCK=0 "
        # primary seeded mid-field aimed up (dir 0x30); extras inherit 0x30.
        f"BATTY_REPLAY_BALL_OBJECT={ball_seed(124, 110, 0x30, 6)}"
    )
    for frame in (4, 8, 12, 16):
        p = boot(mb_env, frame, "multiball")
        fails += check(p, ("object_ball_1", "object_ball_2", "object_ball_3"),
                       frame, "multiball")
        act = p.get("effects_state", "")
        print(f"  multiball f{frame}: {act}  "
              f"[{'ok' if not fails else 'see fails'}]")

    # --- magnet: captured ball curving against the L2 pocket bricks ---
    mag_env = (
        "BATTY_LEVEL=2 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        "BATTY_LAFFC=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} BATTY_REPLAY_BALL_STUCK=0 "
        f"BATTY_REPLAY_BALL_OBJECT={ball_seed(124, 0x40, 0x10, 2)} "
        "BATTY_REPLAY_MAGNET=1"
    )
    for frame in (8, 16, 24, 32):
        p = boot(mag_env, frame, "magnet")
        fails += check(p, ("object_ball_1",), frame, "magnet")
        b1 = bytes.fromhex(p.get("object_ball_1", "00" * 22))
        print(f"  magnet f{frame}: ball x={b1[2]} y={b1[4]} dir=0x{b1[6]:02X}  "
              f"[{'ok' if not fails else 'see fails'}]")

    print()
    if fails:
        print(f"FAIL ball_paths_no_tunnel: {len(fails)} penetration(s)")
        for f in fails:
            print(f"    {f}")
        return 1
    print("PASS ball_paths_no_tunnel: extra balls + magnet-captured ball never "
          "sit inside a solid brick (shared LAFFC path)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
