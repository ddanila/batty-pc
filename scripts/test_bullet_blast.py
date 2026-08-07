#!/usr/bin/env python3
"""Bullet-impact blast animation gate (port-only, RNG-independent).

When a laser bullet stops against a brick or alien it leaves a 4-frame
impact blast (spr_bullet_blast_1..4) played at 2 ticks/frame:
step_bullet_blast decrements bullet_blast_ticks 1/frame from 8 to 0, and the
rendered frame is (ticks-1)/2. Cosmetic, but the last visible per-frame
animation left ungated; this guards the 8-tick duration + 2-ticks/frame
cadence.

Bakes a blast at full duration (BATTY_REPLAY_BLAST=x,y) with the ball
hidden + no-ball-death suppressed, then probes a new `blast_state` line and
asserts the countdown + derived frame:
  f1 -> ticks 7 frame 3, f3 -> 5/2, f5 -> 3/1, f7 -> 1/0  (f8 -> inactive).

Deterministic (the baked/hidden-ball scenario is frame-exact). ZEsarUX-free
(port-only); needs QEMU + mtools. See src/main.cpp step_bullet_blast /
render_bullet_blast.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

# (probe frame, expected ticks, expected derived frame) — countdown from 8.
CASES = [(1, 7, 3), (3, 5, 2), (5, 3, 1), (7, 1, 0)]


def probe_blast(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    probe = ROOT / "build/PROBE_blast.txt"
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_HIDE_BALL=1 "
        f"BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_BLAST=80,80 "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_blast"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r"blast_state=ticks([0-9A-Fa-f]{2})_frame([0-9A-Fa-f]{2})",
                  probe.read_text())
    if not m:
        return None
    return int(m.group(1), 16), int(m.group(2), 16)


def main() -> int:
    ok = True
    for frame, eticks, eframe in CASES:
        r = probe_blast(frame)
        if r is None:
            print(f"  frame {frame}: NO blast_state in PROBE.TXT [FAIL]")
            ok = False
            continue
        ticks, fr = r
        good = (ticks == eticks and fr == eframe)
        ok = ok and good
        print(f"  frame {frame}: ticks={ticks} frame={fr} "
              f"[{'PASS' if good else 'FAIL'}] (expect ticks={eticks} frame={eframe})")
    if ok:
        print("PASS bullet_blast: blast counts down 8->0 at 2 ticks/frame "
              "(frame 3/2/1/0 at f1/3/5/7) — step_bullet_blast guarded")
        return 0
    print("FAIL bullet_blast")
    return 1


if __name__ == "__main__":
    sys.exit(main())
