#!/usr/bin/env python3
"""Deterministic enemy descend-phase gate (port vs original ground truth).

The L3 `l3-brick-flash` original spawns a FRESH alien (sprite_set=$09
anim_bird) at x=168, y=1, dir=$10, spd=1, target($+14)=$10 and slides it
down 1 px/frame while y < 8 (`handling_bird`: `if (y < 8) y++`). This phase
is RNG-INDEPENDENT — no steering re-pick happens until the alien reaches
its target — so it is a clean, deterministic check that the port's
`handling_bird_obj` reproduces the original's descend.

Ground truth (ZEsarUX, `scripts/capture_enemy_flight.py`, frame-pc $BA83):
spawns at y=1 and descends y=1,2,3,... at +1/frame with x/dir/spd/target
held at 168/$10/1/$10 until y>=8. See notes/enemy-movement.md.

This bakes that exact fresh descriptor into the port replay
(BATTY_REPLAY_ENEMY_OBJECT) and probes object_enemy at three frames: two mid-slide, asserting
x=168, dir=$10, spd=1, target=$10 and y advancing +1/frame, and one just
after the slide ends, asserting it has STOPPED at y=8.

The stop-frame case exists because the `y < 8` threshold was otherwise
unguarded across the whole suite — see the comment on CASES.

Note on frame numbering: BATTY_VISUAL_PROBE_FRAMES counts port frames from
the replay start, which is offset by 1 from the original's $BA83 frame
index (a probe-timing convention, not a behaviour difference) — so the
port reads y=frame+1. Only the SLOPE (+1/frame) and the held fields are
asserted, which are convention-independent.

Exit 0 if every checkpoint matches; nonzero otherwise. ZEsarUX-free
(port-only, like test_bat_deflection_port.py); needs QEMU + mtools.
"""
import re
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-test.img")

# Authoritative fresh enemy descriptor (22-byte object_t), read from the
# original at the spawn frame: sprite_set=09, x=A8(168), y=01, dir=10,
# spd=01, w_body=18, h_body=0C, target(+14)=10.
FRESH_ENEMY = "0900A80001001001030FDA35180CA801030FF0701000"
BAT_OBJECT  = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BALL_OBJECT = "02006C004E001F03020CEEF008076C4E020C0000008C"

# (port probe frame, expected y). x/dir/spd/target are frame-invariant
# during the descend. y = probe_frame + 1 (the +1 numbering offset above).
# (port frame, expected y). The first two are mid-slide and check the
# +1/frame slope. The THIRD is the one that pins where the slide STOPS.
#
# `handling_bird` slides while y < 8. Nothing in the suite guarded that
# 8: changing it to 9 left all 59 QEMU gates green, while making the
# alien enter one pixel lower than the original. This gate passed too,
# correctly — its docstring says it asserts the slope and the held
# fields, not the threshold.
#
# Measured on correct code: frame 7 -> y=8 (the slide's last step),
# frame 8 -> y=8 (STOPPED; the descriptor motion has not yet produced a
# whole pixel), frame 9 -> y=9. So frame 8 holding at 8 is the
# discriminator: with the threshold at 9 the slide runs one frame longer
# and y is 9 there.
CASES = [(3, 4), (6, 7), (8, 8)]
EXP_X, EXP_DIR, EXP_SPD, EXP_TARGET = 168, 0x10, 1, 0x10

# Frame 8 is not deterministic in `target`, and asserting $10 there made
# this gate fail about two runs in three (known-bugs #17). Measured, four
# runs of frame 8 alone:
#
#   target=0x10  arrival0_margin0_turns0
#   target=0x29  arrival1_margin0_turns1
#   target=0x29  arrival1_margin0_turns1
#   target=0x10  arrival0_margin0_turns0
#
# Both are CORRECT. Frame 8 is where the entry slide ends, so it is the
# first frame the alien can steer, and steering is gated on
# `pit_frame_counter & 3` — the port's stand-in for the original's
# `counter_misc`, a GLOBAL counter, exactly as the original gates it. Its
# phase when the alien is seeded depends on how long boot took, so under
# load the steer lands on frame 8 or it doesn't. The original is
# phase-dependent here too; see PLAN.md WS6 item 4.
#
# So the gate asserts the IMPLICATION instead of one arm. That is
# stronger than what it replaced, not weaker: it now pins BOTH outcomes,
# and it pins that the slide frames cannot steer at all.
#
#   turns == 0  ->  target is untouched, still $10
#   turns == 1  ->  dir has ARRIVED at target, so the turn re-picked; the
#                   replay RNG is fixed (BATTY_REPLAY_RANDOM=8E49) and
#                   frame 8 is a fixed frame, so the re-pick is $29
#   during the slide (`if (y < 8) { y++; return; }`) no turn can run at
#   all, so turns MUST be 0 on frames 3 and 6
#
# A margin re-pick is wrong at every one of these frames: x=168 is
# nowhere near an edge.
TARGET_AFTER_REPICK = 0x29


def probe_enemy(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_enemy.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} BATTY_REPLAY_BALL_STUCK=0 "
        f"BATTY_REPLAY_BALL_OBJECT={BALL_OBJECT} "
        f"BATTY_REPLAY_ENEMY_OBJECT={FRESH_ENEMY} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_enemy"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    text = probe.read_text()
    m = re.search(r"object_enemy=([0-9A-Fa-f]+)", text)
    if not m:
        return None, None
    r = re.search(r"enemy_repicks=arrival(\d+)_margin(\d+)_turns(\d+)", text)
    if not r:
        raise SystemExit("FAIL: PROBE.TXT has no enemy_repicks line — this "
                         "gate reads it to tell an un-steered frame from a "
                         "steered one (known-bugs #17)")
    counts = tuple(int(g) for g in r.groups())      # arrival, margin, turns
    return bytes.fromhex(m.group(1)), counts


def main() -> int:
    ok = True
    for frame, exp_y in CASES:
        b, counts = probe_enemy(frame)
        if b is None:
            print(f"  frame {frame}: NO enemy in PROBE.TXT [FAIL]")
            ok = False
            continue
        arrival, margin, turns = counts
        sset, x, y, d, spd, tgt = b[0], b[2], b[4], b[6], b[7], b[0x14]

        # The slide returns before the steer, so a turn there is a bug in
        # the port. Only the frame the slide ENDS on may steer.
        max_turns = 0 if frame < 8 else 1
        if turns > max_turns or margin:
            why = f"turns={turns} margin={margin}"
            exp_tgt = "no steer at all"
        elif turns == 0:
            why = "not steered yet"
            exp_tgt = f"0x{EXP_TARGET:02X}"
        else:
            why = "steered: dir had arrived, so it re-picked"
            exp_tgt = f"0x{TARGET_AFTER_REPICK:02X}"

        want_tgt = EXP_TARGET if turns == 0 else TARGET_AFTER_REPICK
        good = (sset == 0x09 and x == EXP_X and y == exp_y and d == EXP_DIR
                and spd == EXP_SPD and tgt == want_tgt
                and turns <= max_turns and margin == 0 and arrival == turns)
        ok = ok and good
        print(f"  frame {frame}: sprite_set={sset:02X} x={x} y={y} "
              f"dir=0x{d:02X} spd={spd} target=0x{tgt:02X} "
              f"arrival={arrival} margin={margin} turns={turns} "
              f"[{'PASS' if good else 'FAIL'}] (expect x={EXP_X} y={exp_y} "
              f"dir=0x{EXP_DIR:02X} spd={EXP_SPD} target={exp_tgt} "
              f"— {why})")
    if ok:
        print("PASS enemy_descend: port handling_bird descend matches the "
              "original (y +1/frame, x/dir/spd held; target follows the "
              "steer count) — GT-validated")
        return 0
    print("FAIL enemy_descend")
    return 1


if __name__ == "__main__":
    sys.exit(main())
