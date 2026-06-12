#!/usr/bin/env python3
"""Enemy fly-over + bomb-drop dirty-redraw gate.

Seeds a fresh-spawn UFO at (64,1) descending for 50 deterministic
frames (single probe-halt checkpoint per boot — multi-checkpoint A/B is
invalid, lessons.md) and compares the dirty-redraw screen against a
FORCE_FULL_FLUSH_EACH_FRAME baseline. By frame 50 the UFO has dropped a
bomb that still overlaps its parent — the exact scenario that exposed
THREE bugs during the 2026-06-11/12 triage (notes/bird-render-parity.md):

1. restore_top_frame_center ran AFTER the object compose in the full
   path, erasing sprite slices over the top-frame centre (fixed).
2. The simple and full compose paths drew the bomb and the enemy in
   OPPOSITE order, so the bomb/UFO overlap rendered differently per
   path — a deterministic 21 px A/B delta at identical probed object
   state (fixed: both paths now follow the original's $9AD0 slot-paint
   order — balls < bullets < bomb/bonus/pts400 < enemy < rocket).
3. The frame-100-class divergences in earlier variants were the
   multi-checkpoint counter-phase artifact, not bugs.

The gate locks 0 px: any future drift between the two compose paths
around a flying alien + falling bomb fails here.

    make test-enemy-flyover-redraw
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
OUT = Path("build/test_enemy_flyover_redraw")
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
        make_diff_png(dirty, full, OUT / "flyover_redraw_diff.png")
        raise SystemExit(
            f"FAIL: enemy fly-over + bomb overlap leaves {diff} px of dirty-path "
            f"residue vs the full-flush baseline [roi {ROI}] "
            f"(diff -> {OUT}/flyover_redraw_diff.png)"
        )
    print(f"PASS enemy_flyover_redraw: dirty redraw leaves no fly-over "
          f"residue around the alien + bomb [roi {ROI}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
