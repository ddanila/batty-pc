#!/usr/bin/env python3
"""Bonus catch->effect gate (port-only, deterministic).

Catching a bonus writes its original type code into bat.bonus_applied
(object_bat_1 +0x14) for every type except ROCKET (get_bonus / LA67B_3 at
$A6FC) — the byte that downstream effects read (LASER enables firing,
MAGNET/CATCH sticks the ball, BIG_BAT widens, KILL_ALIENS=0x09 stops alien
spawns). This catch->effect linkage was code-comparison-verified but
ungated.

Bakes a bonus of a given PORT type just above the bat (BATTY_REPLAY_BONUS=
type,x,y) so it falls into the catch band on frame 1, with the ball hidden
+ no-ball-death suppressed (the catch is ball-independent). Probes
object_bat_1.bonus_applied at f2 and asserts it equals the DOCUMENTED
original code for that type (from the $9F.. bonus-code map: 0x00 BIG_BAT,
0x01 LASER/gun, 0x03 MAGNET, 0x09 KILL_ALIENS) — hardcoded here, not read
from the C our_to_orig_bonus, so a wrong mapping is caught. Only types whose
code differs from the 0xFF level-entry value are used, so a registered
catch is unambiguous. Composes with test-laser-cadence (which bakes
bonus_applied=0x01 directly): catch LASER -> 0x01 -> fires.

ZEsarUX-free (port-only); needs QEMU + mtools. See src/main.c bonus_apply /
our_to_orig_bonus.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

# (port bonus type, expected bat.bonus_applied = documented original code).
CASES = [
    (8, 0x01, "LASER"),
    (5, 0x03, "CATCH/MAGNET"),
    (2, 0x00, "BIG_BAT"),
    (4, 0x09, "KILL_ALIENS"),
]


def probe_bonus_applied(port_type: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_eff.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_BONUS={port_type},118,167 "
        f"BATTY_VISUAL_PROBE_FRAMES=2"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", "2", "--wait-key",
                    "--out", "build/tl_eff"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"object_bat_1=([0-9A-Fa-f]+)", probe.read_text())
    if not m:
        return None
    b = bytes.fromhex(m.group(1))
    return b[0x14] if len(b) > 0x14 else None


def main() -> int:
    ok = True
    for port_type, expected, name in CASES:
        ba = probe_bonus_applied(port_type)
        if ba is None:
            print(f"  {name} (type {port_type}): NO object_bat_1 in PROBE.TXT [FAIL]")
            ok = False
            continue
        good = (ba == expected)
        ok = ok and good
        print(f"  catch {name} (port type {port_type}): bonus_applied=0x{ba:02X} "
              f"[{'PASS' if good else 'FAIL'}] (expect 0x{expected:02X})")
    if ok:
        print("PASS bonus_effects: each catch writes the documented original "
              "code into bat.bonus_applied — catch->effect linkage guarded")
        return 0
    print("FAIL bonus_effects")
    return 1


if __name__ == "__main__":
    sys.exit(main())
