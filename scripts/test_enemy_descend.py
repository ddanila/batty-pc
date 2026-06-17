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
(BATTY_REPLAY_ENEMY_OBJECT) and probes object_enemy at two descend frames,
asserting x=168, dir=$10, spd=1, target=$10 and y advancing +1/frame.

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
CASES = [(3, 4), (6, 7)]
EXP_X, EXP_DIR, EXP_SPD, EXP_TARGET = 168, 0x10, 1, 0x10


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
    m = re.search(r"object_enemy=([0-9A-Fa-f]+)", probe.read_text())
    return bytes.fromhex(m.group(1)) if m else None


def main() -> int:
    ok = True
    for frame, exp_y in CASES:
        b = probe_enemy(frame)
        if b is None:
            print(f"  frame {frame}: NO enemy in PROBE.TXT [FAIL]")
            ok = False
            continue
        sset, x, y, d, spd, tgt = b[0], b[2], b[4], b[6], b[7], b[0x14]
        good = (sset == 0x09 and x == EXP_X and y == exp_y and d == EXP_DIR
                and spd == EXP_SPD and tgt == EXP_TARGET)
        ok = ok and good
        print(f"  frame {frame}: sprite_set={sset:02X} x={x} y={y} "
              f"dir=0x{d:02X} spd={spd} target=0x{tgt:02X} "
              f"[{'PASS' if good else 'FAIL'}] (expect x={EXP_X} y={exp_y} "
              f"dir=0x{EXP_DIR:02X} spd={EXP_SPD} target=0x{EXP_TARGET:02X})")
    if ok:
        print("PASS enemy_descend: port handling_bird descend matches the "
              "original (y +1/frame, x/dir/spd/target held) — GT-validated")
        return 0
    print("FAIL enemy_descend")
    return 1


if __name__ == "__main__":
    sys.exit(main())
