#!/usr/bin/env python3
"""Double Play: two bats, two key clusters, and neither crosses the net.

The original splits one keyboard between two players — ASDFG for the
left bat, HJKL+Enter for the right — and then confines each bat to its
own half of the court:

    get_left_player_ctrl_state  $A161   $FDFE  A,D = LEFT   S,F = RIGHT
    get_right_player_ctrl_state $A19E   $BFFE  J,L = LEFT   K,Ent = RIGHT
    LD IX,object_bat_1 / CALL LACCE     $ACCE  right edge stops at $80
    LD IX,object_bat_2 / CALL LACAD     $ACAD  left  edge stops at $80

Both halves are needed for either to be worth anything. Input without
the clamps lets bat 1 walk through the separator and across bat 2's
court; clamps without input leave bat 2 parked at $B0 forever, which is
where it sat until this landed.

### The scenario

Mode 2, level 1, ball held on the bat so nothing dies. Bat 1 starts at
$38 and bat 2 at $B0 — `new_game_reset`'s mode-2 placement. Both players
are then driven at the divider from opposite sides and given 20 frames,
comfortably more than the 11 and 12 they need to arrive:

  subject  hold S (P1 right) and J (P2 left) -> both pin at the divider,
           bat 1 with its RIGHT edge on $80 (so x = $80 - $1C = $64) and
           bat 2 with its LEFT edge on $80 (x = $80)
  control  same keys, mode 0 -> bat 1 does not move at all, because the
           ASDFG cluster is Double-Play-only; bat 2 does not exist

The asymmetry between the two clamps is the part worth gating. Written
symmetrically — both comparing x — bat 1 would overlap the separator by
its own 28 px and the two bats would share a stripe of court.

### Why the keys are held rather than typed

`BATTY_HOLD_KEYS` seeds key_state[] directly. The capture harness runs
headless, so INT 9 never fires and a seeded key is never released — a
held key is the only kind this harness can express, and steering is the
only thing that needs one.

The third probe pins the arrow keys, which are the port's one addition:
the original takes the arrows away from player 1 in mode 2 and puts both
players on the split keyboard. On a PC the arrows sit by the numpad, far
from HJKL, so leaving them live costs player 2 nothing. It is a superset
and it is deliberate, so it gets a row here rather than a footnote.

### Why ENTER is NOT one of bat 2's keys

The original's right cluster is K *or* Enter ($BFFE bit 0). The port
drops Enter, and the last row above is what pins that: with no P2 key
held, bat 2 must be EXACTLY on its seed.

ENTER is this port's attract-chain affordance (PLAN.md WS1) and the
capture harness presses it to start every run, so binding it to bat 2
made the harness's own keystroke nudge the bat 4 px at a moment nothing
controls. That first showed up not here but in `test-double-play-court`,
which measures bat 2's pixel EXTENT and began flaking between 207 and
211 — one bat step apart — depending on whether the key-up landed
inside the capture window.

The row here originally ALLOWED both $B0 and $B4 and called the race
honest bounding. It was not: a gate written around a defect makes the
defect permanent, and the defect was real outside the harness too — a
player pressing ENTER to get through a screen would move bat 2 in the
next level. Fixing the binding was the answer; widening the assertion
had merely hidden it from the one gate that could see it.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-dpin.img")
FRAME = 20

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"

SC = {"S": 0x1F, "F": 0x21, "A": 0x1E, "D": 0x20,
      "J": 0x24, "K": 0x25, "L": 0x26,
      "LEFT": 0x4B, "RIGHT": 0x4D}

# new_game_reset's mode-2 placement. The bat object has to be SEEDED
# there: BATTY_REPLAY_BAT_OBJECT overrides the placement, and its stock
# x of $74 already straddles the divider, so bat 1 would arrive pinned
# at $64 whether or not a key was ever read. The first draft did exactly
# that and passed with the whole input path dead.
BAT1_SEED = 0x38
DIVIDER, BAT_W = 0x80, 0x1C
WALL_L, WALL_R = 0x08, 0xDC      # bat_step_x's playfield clamps


def probe(mode: str, keys, bat1_x: int = BAT1_SEED):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_dpin.txt"
    out.unlink(missing_ok=True)
    hold = ",".join(f"{SC[k]:02X}" for k in keys)
    b = bytearray(bytes.fromhex(BAT))
    b[2] = bat1_x
    bat = b.hex().upper()
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 "
        f"BATTY_GAME_MODE={mode} BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 "
        f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_COUNTER=0 "
        f"BATTY_REPLAY_BALL_STUCK=1 BATTY_REPLAY_BAT_OBJECT={bat} "
        f"BATTY_HOLD_KEYS={hold} BATTY_VISUAL_PROBE_FRAMES={FRAME}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(FRAME), "--wait-key",
                    "--out", "build/tl_dpin"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    text = out.read_text()
    m1 = re.search(r"object_bat_1=([0-9A-Fa-f]+)", text)
    m2 = re.search(r"object_bat_2=([0-9A-Fa-f]+)", text)
    if not m1 or not m2:
        raise SystemExit("FAIL: PROBE.TXT has no object_bat_1/object_bat_2 "
                         "row — if probe_write_object's names changed, "
                         "point this gate at them")
    return bytes.fromhex(m1.group(1))[2], bytes.fromhex(m2.group(1))[2]


def check(label, got, want_x1, want_x2, why):
    """want_x2 is None where bat 2 is inactive, or a tuple where it races."""
    if got is None:
        print(f"  {label}: NO PROBE.TXT [FAIL]")
        return False
    x1, x2 = got
    allowed = want_x2 if isinstance(want_x2, tuple) else (want_x2,)
    ok = (x1 == want_x1) and (want_x2 is None or x2 in allowed)
    if want_x2 is None:
        want2 = "----"
    else:
        want2 = "|".join(f"${v:02X}" for v in allowed)
    print(f"  {label}: bat1=${x1:02X} bat2=${x2:02X} "
          f"[{'PASS' if ok else 'FAIL'}] (expect ${want_x1:02X}/{want2}"
          f" — {why})")
    return ok


def main() -> int:
    ok = True

    ok &= check("mode 2, S + J    ", probe("2", ("S", "J")),
                DIVIDER - BAT_W, DIVIDER,
                "both driven INTO the divider and stopped by it — without "
                "the clamps they would be at $88 and $60, through it")

    ok &= check("mode 2, A + K    ", probe("2", ("A", "K")),
                WALL_L, WALL_R,
                "the same two clusters driven the other way, out to the "
                "playfield walls; no court clamp is involved")

    ok &= check("mode 0, S + J    ", probe("0", ("S", "J")),
                BAT1_SEED, None,
                "the ASDFG cluster is Double-Play-only, so bat 1 has not "
                "moved from its seed; bat 2 is inactive")

    ok &= check("mode 2, RIGHT    ", probe("2", ("RIGHT",)),
                DIVIDER - BAT_W, 0xB0,
                "player 1's arrows still steer in Double Play — the port's "
                "own addition; dead arrows would leave bat 1 on its seed. "
                "Bat 2 is EXACTLY on its seed: see below")

    if ok:
        print("PASS double_play_input: both clusters steer their own bat "
              "and the divider holds")
        return 0
    print("FAIL double_play_input")
    return 1


if __name__ == "__main__":
    sys.exit(main())
