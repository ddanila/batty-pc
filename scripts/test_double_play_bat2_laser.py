#!/usr/bin/env python3
"""Player 2 fires the laser from BAT 2, and only when bat 2 holds it.

`free_bullet_2` ($A14C) reads the firing bat out of IX —

    LD A,(IX+$02) / ADD A,$0C / LD (IY+$02),A

— so the bullet leaves whichever bat fired, 12 px in from its left edge.
Nothing about it is bat 1's. The port's `try_fire_laser` had bat 1
hardcoded in all three places: the bonus byte it gates on, the x it
fires from, and the muzzle-flash counter it sets.

### Player 2's fire keys

The original reads them as one combined half-row, `$5FFE` = `$7FFE |
$DFFE` — Y U I O P together with B N M, SYMBOL SHIFT and SPACE, `AND
$1F`, so any of them fires.

SPACE is the one key not carried over: this port committed it to player
1's fire long before Double Play existed, and both `test-visual` and
`test-normal-ball-launch` press it. Same call as dropping ENTER from
bat 2's RIGHT — transcribe the cluster, minus the key already spent.

Polled from `key_state` rather than the BIOS buffer, because one key
queue cannot serve two players, and because polling every frame is what
the original does anyway.

### The scenario

Mode 2, ball held so nothing else moves, Y held down throughout:

  armed    a LASER bonus dropped on BAT 2 -> shots fired, and the
           bullet's x is bat 2's + 12
  control  no bonus -> nothing fires, however long Y is held

The bullet x is the half that matters. A bat-1-sourced bullet would
still register as "a shot happened" — the count alone cannot tell which
bat fired, and the first draft of this gate checked only the count.

### Shared on purpose

The bullet pool and the cooldown stay global. That is faithful: the
original's `bullet` counter is a single byte and `free_bullet_2` is
handed a bat, so two armed bats compete for the same two slots and the
same cadence.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-b2laser.img")
FRAME = 40

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
SC_Y = "15"
BAT2_X = 0xB0
MUZZLE_DX = 12               # free_bullet_2's ADD A,$0C
# LASER (port type 8) seeded already overlapping bat 2, so it is caught
# on frame 1 rather than after a slow fall.
LASER_ON_BAT2 = "8,186,168"


def probe(bonus: str | None):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_b2laser.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 BATTY_LEVEL=1 "
        f"BATTY_GAME_MODE=2 BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 "
        f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_COUNTER=0 "
        f"BATTY_REPLAY_BALL_STUCK=1 BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT} BATTY_HOLD_KEYS={SC_Y} "
        f"{f'BATTY_REPLAY_BONUS={bonus} ' if bonus else ''}"
        f"BATTY_VISUAL_PROBE_FRAMES={FRAME}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(FRAME), "--wait-key",
                    "--out", "build/tl_b2laser"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    t = out.read_text()
    shots = re.search(r"laser_fire_state=shots([0-9A-Fa-f]{4})", t)
    blt = re.search(r"bullet_state=active([0-9A-Fa-f]{2})_x([0-9A-Fa-f]{2})", t)
    b2 = re.search(r"object_bat_2=([0-9A-Fa-f]+)", t)
    if not shots or not blt or not b2:
        raise SystemExit("FAIL: PROBE.TXT is missing laser_fire_state, "
                         "bullet_state or object_bat_2")
    return (int(shots.group(1), 16), int(blt.group(1), 16),
            int(blt.group(2), 16), bytes.fromhex(b2.group(1))[2])


def main() -> int:
    ok = True

    got = probe(LASER_ON_BAT2)
    if got is None:
        print("  armed: NO PROBE.TXT [FAIL]")
        return 1
    shots, active, bx, b2x = got
    want_x = (b2x + MUZZLE_DX) & 0xFF
    good = shots > 0 and active == 1 and bx == want_x
    ok = ok and good
    print(f"  LASER on bat 2: shots={shots} bullet(active={active}, "
          f"x=${bx:02X}) bat2=${b2x:02X} [{'PASS' if good else 'FAIL'}] "
          f"(expect shots>0 and the bullet at ${want_x:02X} = bat 2's x "
          f"+ {MUZZLE_DX} — a bat-1 bullet would still count as a shot)")

    got = probe(None)
    if got is None:
        print("  control: NO PROBE.TXT [FAIL]")
        return 1
    shots, active, bx, b2x = got
    good = shots == 0 and active == 0
    ok = ok and good
    print(f"  no bonus:       shots={shots} bullet(active={active}) "
          f"[{'PASS' if good else 'FAIL'}] (expect nothing — Y is held "
          f"just as hard, but bat 2 carries no LASER)")

    if ok:
        print("PASS double_play_bat2_laser: bat 2 fires its own laser, from "
              "its own x, only when armed")
        return 0
    print("FAIL double_play_bat2_laser")
    return 1


if __name__ == "__main__":
    sys.exit(main())
