#!/usr/bin/env python3
"""REPRO (expected-FAIL today): enemy fly-over leaves a small trailing
residue on the dirty-redraw path.

Found 2026-06-11 while triaging the L3/L9 state4 drift (which turned
out to be the screendump racing the LIVE alien — see
notes/per-level-profile.md; the top band itself heals correctly). This
A/B harness, however, surfaced a real, separate defect: a fresh-spawn
UFO seeded at (64, 1) descending for 50 deterministic frames leaves
~21 px of black trailing residue at its upper edge rows (measured
(83..87, 49..57), dirty=black vs full-flush=texture ink) — the dirty
path's handling of the vacated rows misses pixels the full-compose
baseline renders correctly.

Both boots halt at the SAME game frame (VISUAL_PROBE_FRAMES=50) with
RNG + counter phase pinned, so the diff is a pure render-path delta,
not timing. NOT wired into any suite yet — this is the repro for the
next fix; once green, rename to test_* and wire into parity-check-full.

    python3 scripts/repro_enemy_flyover_trail.py
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import PALETTE_RGB, make_diff_png, ppm_inner_to_indices, run_qemu


TEST_FLOPPY = Path("build/batty-test.img")
OUT = Path("build/repro_enemy_flyover_trail")
# Fresh-spawn UFO, exactly the enemy_prepare ($9EAA) state: sprite_set=08,
# x=0x40 (the drifting levels' spawn slot), y=1, dir=$10 (down), speed 1,
# body 24x16, misc_12/13 = prop_even $60/$90, target (+$14) = $10.
ENEMY_OBJECT = "08004000010010010000000018100000000060901000"
FRAMES = "50"
ROI = (0, 0, 256, 192)


def build_floppy(force_full_flush: bool) -> None:
    env = os.environ.copy()
    env.update(
        {
            "BATTY_START_LEVEL": "1",
            "BATTY_REPLAY_WAIT_KEY": "1",
            "BATTY_REPLAY_ENEMY_OBJECT": ENEMY_OBJECT,
            "BATTY_VISUAL_PROBE_FRAMES": FRAMES,
            # Pin RNG + counter phase so both boots steer the UFO (and
            # gate bomb_appear) identically — see the
            # test-ball-object-dirty-redraw lesson in notes/lessons.md.
            "BATTY_REPLAY_RANDOM": "8E49",
            "BATTY_REPLAY_COUNTER": "0",
        }
    )
    if force_full_flush:
        env["BATTY_FORCE_FULL_FLUSH_EACH_FRAME"] = "1"
    else:
        env.pop("BATTY_FORCE_FULL_FLUSH_EACH_FRAME", None)
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def capture(label: str) -> bytes:
    out = OUT / label
    out.mkdir(parents=True, exist_ok=True)
    ppm = out / "after_frames.ppm"
    script = [
        "SLEEP 9.0",
        "sendkey ret",
        "SLEEP 2.0",
        f"screendump {ppm}",
        "sendkey esc",
        "SLEEP 0.2",
    ]
    run_qemu(TEST_FLOPPY, script, out / "qemu.log")
    return ppm_inner_to_indices(ppm)


def roi_diff(a: bytes, b: bytes, roi) -> int:
    x0, y0, x1, y1 = roi
    return sum(
        1
        for y in range(y0, y1)
        for x in range(x0, x1)
        if PALETTE_RGB[a[y * 256 + x]] != PALETTE_RGB[b[y * 256 + x]]
    )


def main() -> int:
    shutil.rmtree(OUT, ignore_errors=True)
    OUT.mkdir(parents=True, exist_ok=True)

    build_floppy(force_full_flush=False)
    dirty = capture("dirty")
    build_floppy(force_full_flush=True)
    full = capture("full")

    diff = roi_diff(dirty, full, ROI)
    if diff != 0:
        make_diff_png(dirty, full, OUT / "flyover_trail_diff.png")
        raise SystemExit(
            f"REPRODUCED (expected): enemy fly-over leaves {diff} px of dirty-path "
            f"residue vs the full-flush baseline [roi {ROI}] "
            f"(diff -> {OUT}/flyover_trail_diff.png)"
        )
    print(f"PASS (bug fixed?): dirty redraw leaves no fly-over "
          f"residue in the top band [roi {ROI}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
