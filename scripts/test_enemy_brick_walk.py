#!/usr/bin/env python3
"""An alien that hits a brick walks to the snap point and breaks nothing.

`LAFFC_30` tests the object's sprite_set and only `$02` — the ball —
takes the destroy-and-score path (`LAFFC_32`). An alien reaching the
same code gets `$08`/`$09`, which falls through to

    LD L,(IX+$02) / LD H,(IX+$04) / LD (LAA7B),HL
    LB1C3: LD HL,<entry position>   ; self-modified at LAFFC_7
           LD (IX+$02),L / LD (IX+$04),H / RET

so the snapped position is RECORDED and then UNDONE. `handling_bird`
sees the latched word at its top and diverts onto `LAA44`, which walks
the alien there a pixel per axis per frame with steering, movement and
collision all skipped. See notes/enemy-movement.md.

WHY THIS GATE HAD TO EXIST BEFORE THE CODE COULD BE BELIEVED. The whole
reaction is invisible on screen: the bricks are untouched BY DESIGN, and
the alien ends up where a bounce would have put it anyway. All 75 gates
passed both before and after wiring it up. The only reason this one can
see anything is that `enemy_home` was added to PROBE.TXT for it.

The scenario seeds a fresh alien inside the L3 brick band (y=$3C) aimed
straight down (dir=$10, target=$10 so nothing steers it away) at x=164,
and reads four frames.

WHAT IS AND IS NOT ASSERTED. `target` is deliberately not checked. The
alien's first frame is ordinary flight, and a steer there is gated on
`pit_frame_counter & 3` — a global counter whose phase depends on how
long boot took (known-bugs #17). It happens to be stable over the three
runs measured, but that is luck, not a property. x, y, dir and the
latched word are phase-independent, because from frame 1 on the alien is
homing and homing skips the steer entirely.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-brickwalk.img")

# Fresh alien, dropped INTO the band: sprite_set=09, x=$A4 (164),
# y=$3C (60), dir=$10 (straight down), spd=1, w=$18 h=$0C, target=$10.
ENEMY = "0905A4473C641001030FDD74180CA41C030F30701000"
BAT   = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BALL  = "02006C004E001F03020CEEF008076C4E020C0000008C"

# (frame, x, y, dir, home) — measured on the ported code and cross-read
# against the disassembly above.
#
#   frame 1  the alien moves, overlaps a brick, and latches: the snap
#            point is ($A8,$3D) and the alien is left at x=164
#   frames 2-4  the walk. y is already at the target, so only x moves,
#            one pixel per frame: 165, 166, 167
#   frame 5  x reaches $A8 and LAA44_4 clears the word, so frame 6 is
#            ordinary flight again — and it re-latches, at ($A8,$34),
#            with dir now reflected to $30. That frame is NOT asserted:
#            it follows a normal-flight frame and so is phase-exposed.
CASES = [
    (2, 165, 61, 0x10, "A83D"),
    (3, 166, 61, 0x10, "A83D"),
    (4, 167, 61, 0x10, "A83D"),
]


def probe(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_brickwalk.txt"
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
                    "--out", "build/tl_brickwalk"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    text = out.read_text()
    m = re.search(r"object_enemy=([0-9A-Fa-f]+)", text)
    h = re.search(r"enemy_home=([0-9A-Fa-f]{4})", text)
    q = re.search(r"bricks_quantity=([0-9A-Fa-f]+)", text)
    if not h:
        raise SystemExit(
            "FAIL: PROBE.TXT has no enemy_home line. That word IS this "
            "gate — the alien's brick reaction leaves no other trace, by "
            "design, so without it there is nothing to assert.")
    if not m or not q:
        return None
    return bytes.fromhex(m.group(1)), h.group(1).upper(), int(q.group(1), 16)


def main() -> int:
    ok = True
    bricks = set()
    for frame, exp_x, exp_y, exp_dir, exp_home in CASES:
        r = probe(frame)
        if r is None:
            print(f"  frame {frame}: NO enemy in PROBE.TXT [FAIL]")
            ok = False
            continue
        b, home, quantity = r
        bricks.add(quantity)
        x, y, d = b[2], b[4], b[6]
        good = (x == exp_x and y == exp_y and d == exp_dir
                and home == exp_home)
        ok = ok and good
        print(f"  frame {frame}: x={x} y={y} dir=0x{d:02X} home={home} "
              f"bricks={quantity} [{'PASS' if good else 'FAIL'}] "
              f"(expect x={exp_x} y={exp_y} dir=0x{exp_dir:02X} "
              f"home={exp_home})")

    # The parity claim the whole reaction rests on: sprite_set $09 never
    # reaches LAFFC_32, so an alien sitting on a brick for three frames
    # destroys nothing.
    if len(bricks) > 1:
        print(f"  bricks_quantity changed across the walk: "
              f"{sorted(bricks)} [FAIL] — an alien took the ball's "
              f"destroy path (LAFFC_32); only sprite_set $02 may")
        ok = False

    if ok:
        print("PASS enemy_brick_walk: alien latches the snap point, walks "
              "1px/frame, and breaks no brick")
        return 0
    print("FAIL enemy_brick_walk")
    return 1


if __name__ == "__main__":
    sys.exit(main())
