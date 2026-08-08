#!/usr/bin/env python3
"""Gate the MULTI-checkpoint visual probe path.

Every other gate sets BATTY_VISUAL_PROBE_FRAMES to a single frame and
rebuilds the floppy when it wants another one. So nothing exercised the
two things visual_checkpoint_tick does that a single checkpoint never
reaches: the delta between consecutive checkpoints, and resuming play
after one that is not the last.

Break the resume and the single-checkpoint gates stay green — the run
simply stops at the first checkpoint and every existing gate is
satisfied. Mutation-checked: stopping after the second checkpoint fails
this gate.

WHAT THIS DOES NOT CHECK: the delta arithmetic itself. The capture tool
names each file after the REQUESTED frame, not the frame that actually
fired, so treating the delta as an absolute (capturing 20, 60, 120
instead of 20, 40, 60) still produces all three files and passes here.
Catching that needs a scenario with motion AND an expected image per
checkpoint, which is a golden-capture gate rather than this one. Said
out loud because a green run here is easy to over-read.

This is not one of the timing-sensitive gates. The port HALTS at each
checkpoint waiting for a key, so the captures are deterministic by
construction rather than by sleeping long enough (contrast
test-bat-redraw-window, notes/testing.md).
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import test_floppy

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = test_floppy()
OUT = Path("build/test_visual_checkpoints")
CHECKPOINTS = [20, 40, 60]


def build_floppy() -> None:
    env = os.environ.copy()
    env.update({
        "BATTY_START_LEVEL": "1",
        "BATTY_LEVEL": "1",
        "BATTY_REPLAY_PROBE": "1",
        "BATTY_REPLAY_WAIT_KEY": "1",
        "BATTY_VISUAL_PROBE_FRAMES": ",".join(str(f) for f in CHECKPOINTS),
    })
    Path(FLOPPY).unlink(missing_ok=True)
    subprocess.run(["make", FLOPPY], check=True, cwd=ROOT, env=env,
                   stdout=subprocess.DEVNULL)


def main() -> int:
    build_floppy()
    proc = subprocess.run(
        [sys.executable, "scripts/capture_frame_timeline.py",
         "--floppy", FLOPPY,
         "--frames", ",".join(str(f) for f in CHECKPOINTS),
         "--wait-key", "--out", str(OUT)],
        cwd=ROOT, capture_output=True, text=True)
    log = proc.stdout + proc.stderr

    if proc.returncode != 0:
        sys.stdout.write(log)
        raise SystemExit("FAIL: multi-checkpoint capture did not complete")

    # Every checkpoint must have produced its own frame. A broken delta
    # stops the run early, so the later files simply never appear.
    missing = [f for f in CHECKPOINTS if not (OUT / f"frame_{f:04d}.ppm").exists()]
    if missing:
        sys.stdout.write(log)
        raise SystemExit(f"FAIL: no capture for checkpoint(s) {missing} — the "
                         f"run stopped before reaching them")
    if not (OUT / "frame_0000.ppm").exists():
        raise SystemExit("FAIL: --wait-key did not capture the aligned frame 0")

    print(f"PASS visual_checkpoints: {len(CHECKPOINTS)} checkpoints "
          f"{CHECKPOINTS} each captured, plus the aligned frame 0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
