#!/usr/bin/env python3
"""Byte-exact RNG-walk gate (port per-frame tick vs original ground truth).

The original ticks `random_generate` ($0072) once per frame at the main
loop top: `E += src + $05 + ctrl_btns; D += ~src + $16 + L; random_number
= DE; random_seed = (random_seed + 1) & $9FFF` where `src = mem[random_
seed]` (a byte of the game's own $8000-$9FFF image) and L is the seed low
byte. The port's `next_random()` reproduces this with `random_rom` (the
$8000-$9FFF dump in random_seed.bin) and the per-frame tick gated by
`BATTY_RNG_PERFRAME=1`.

Ground truth (ZEsarUX, reading $8D48/$8D4A at each $BA83 boundary from the
L3 `l3-brick-flash` state):

    f0  random_number=3793  seed=962A   (the seeded start)
    f1  random_number=BB53  seed=962B
    f2  random_number=460D  seed=962C
    f3  random_number=0990  seed=962D
    f4  random_number=6A76  seed=962E

(random_number is shown D:E = high:low, the order the port's PROBE prints
`random_number=%02X%02X` of (random_d, random_e); the original's $8D48/$8D49
bytes are E then D, i.e. the byte-reversed pair — getting that order wrong
is exactly what made an earlier investigation think the walk diverged.)

This bakes the byte-correct f0 seed (RANDOM=3793, RANDOM_SEED=962A) with
the per-frame tick ON and probes `random_number` at a few frames, asserting
it matches the original walk byte-for-byte. Proves the RNG model + the
random_seed.bin source + the tick are byte-exact, which is the prerequisite
for RNG-dependent parity (enemy steering target, bonus drops).

ZEsarUX-free (port-only); needs QEMU + mtools. See notes/rng-model.md.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

# f0 seed (byte-correct): random_number D:E = 37:93 -> RANDOM=3793;
# random_seed (LE 16-bit) = 962A.
SEED_RANDOM = "3793"
SEED_RANDOM_SEED = "962A"
BAT_OBJECT  = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BALL_OBJECT = "02006C004E001F03020CEEF008076C4E020C0000008C"
FRESH_ENEMY = "0900A80001001001030FDA35180CA801030FF0701000"

# (probe frame, expected random_number as the port prints it = D:E).
CASES = [(1, "BB53"), (2, "460D"), (4, "6A76")]


def probe_rng(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_rng.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_RNG_PERFRAME=1 "
        f"BATTY_REPLAY_RANDOM={SEED_RANDOM} "
        f"BATTY_REPLAY_RANDOM_SEED={SEED_RANDOM_SEED} "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} BATTY_REPLAY_BALL_STUCK=0 "
        f"BATTY_REPLAY_BALL_OBJECT={BALL_OBJECT} "
        f"BATTY_REPLAY_ENEMY_OBJECT={FRESH_ENEMY} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_rng"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"random_number=([0-9A-Fa-f]+)", probe.read_text())
    return m.group(1).upper() if m else None


def main() -> int:
    ok = True
    for frame, exp in CASES:
        got = probe_rng(frame)
        good = (got == exp)
        ok = ok and good
        print(f"  frame {frame}: random_number={got} "
              f"[{'PASS' if good else 'FAIL'}] (expect {exp})")
    if ok:
        print("PASS rng_walk: port per-frame tick reproduces the original "
              "random_number walk byte-exact (RANDOM=3793 SEED=962A, flag ON)")
        return 0
    print("FAIL rng_walk")
    return 1


if __name__ == "__main__":
    sys.exit(main())
