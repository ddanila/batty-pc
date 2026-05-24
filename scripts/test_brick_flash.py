#!/usr/bin/env python3
"""Regression check for brick-destruction flash cleanup.

Drives the DOS build into L3, releases the ball, waits long enough for
at least one brick interaction, then verifies that no solid bright-white
flash rectangle remains in the brick grid after the flash animation
should have restored the level background, and that at least one
brick-sized cell remains visibly removed after the flash clears.

The stale-flash check is anchored to the original-captured L3 render
(`build/level_gt/level_03.scr`). A few bright pixels are normal in brick
art; a full-cell white block is not.
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import (PALETTE_RGB, PLAYFIELD_W, expected_from_scr,
                         ppm_inner_to_indices, run_qemu)

OUT = Path("build/test_brick_flash")
FLOPPY = Path("build/batty-test.img")
ROWS, COLS = 12, 15
BRICK_X0, BRICK_Y0 = 8, 32
BRICK_W, BRICK_H = 16, 8
BRIGHT_WHITE = (255, 255, 255)
WHITE_MARGIN = 75
DESTROYED_CELL_DIFF_MIN = 80


def capture():
    OUT.mkdir(parents=True, exist_ok=True)
    script = [
        "SLEEP 10",
        "sendkey ret", "SLEEP 1.5",
        "sendkey ret", "SLEEP 1.5",
        "sendkey ret", "SLEEP 1.5",
        "sendkey ret", "SLEEP 1.5",
        "sendkey ret", "SLEEP 1.5",
        f"screendump {OUT}/l3_initial.ppm", "SLEEP 0.3",
        "sendkey spc",
        "SLEEP 12",
        f"screendump {OUT}/l3_after.ppm", "SLEEP 0.3",
        "sendkey esc",
    ]
    run_qemu(FLOPPY, script, OUT / "qemu.log")


def brick_band_diff(a, b):
    diff = 0
    for y in range(BRICK_Y0, BRICK_Y0 + ROWS * BRICK_H):
        row = y * PLAYFIELD_W
        for x in range(BRICK_X0, BRICK_X0 + COLS * BRICK_W):
            if PALETTE_RGB[a[row + x]] != PALETTE_RGB[b[row + x]]:
                diff += 1
    return diff


def white_counts_by_cell(idx):
    counts = []
    for r in range(ROWS):
        for c in range(COLS):
            x0 = BRICK_X0 + c * BRICK_W
            y0 = BRICK_Y0 + r * BRICK_H
            white = 0
            for y in range(y0, y0 + 7):
                row = y * PLAYFIELD_W
                for x in range(x0, x0 + BRICK_W):
                    if PALETTE_RGB[idx[row + x]] == BRIGHT_WHITE:
                        white += 1
            counts.append(white)
    return counts


def stale_flash_cells(actual_idx, original_idx):
    cells = []
    actual = white_counts_by_cell(actual_idx)
    original = white_counts_by_cell(original_idx)
    for i, white in enumerate(actual):
        allowed = original[i] + WHITE_MARGIN
        if white > allowed:
            r, c = divmod(i, COLS)
            cells.append((r, c, white, original[i], allowed))
    return cells


def changed_cells_by_cell(before_idx, after_idx):
    cells = []
    for r in range(ROWS):
        for c in range(COLS):
            x0 = BRICK_X0 + c * BRICK_W
            y0 = BRICK_Y0 + r * BRICK_H
            changed = 0
            for y in range(y0, y0 + BRICK_H):
                row = y * PLAYFIELD_W
                for x in range(x0, x0 + BRICK_W):
                    if PALETTE_RGB[before_idx[row + x]] != PALETTE_RGB[after_idx[row + x]]:
                        changed += 1
            if changed:
                cells.append((r, c, changed))
    cells.sort(key=lambda item: item[2], reverse=True)
    return cells


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--expected-original", default="build/level_gt/level_03.scr",
                    help="original-captured L3 reference screen")
    args = ap.parse_args()

    capture()
    initial = ppm_inner_to_indices(OUT / "l3_initial.ppm")
    after = ppm_inner_to_indices(OUT / "l3_after.ppm")
    original = expected_from_scr(Path(args.expected_original))
    diff = brick_band_diff(initial, after)
    if diff == 0:
        raise SystemExit("FAIL: L3 brick band did not change; test did not exercise brick destruction")
    changed_cells = changed_cells_by_cell(initial, after)
    if not changed_cells or changed_cells[0][2] < DESTROYED_CELL_DIFF_MIN:
        detail = ", ".join(
            f"r{r}c{c}: {changed}" for r, c, changed in changed_cells[:5])
        raise SystemExit(
            "FAIL: no brick-sized cell stayed removed after the hit; "
            f"largest cell diff {detail or 'none'}")
    stale = stale_flash_cells(after, original)
    if stale:
        detail = ", ".join(
            f"r{r}c{c}: actual {white}, original {ref}, allowed {allowed}"
            for r, c, white, ref, allowed in stale)
        raise SystemExit(f"FAIL: stale bright-white brick flash cells vs original L3 reference: {detail}")
    top = ", ".join(f"r{r}c{c}: {changed}" for r, c, changed in changed_cells[:3])
    print(f"PASS brick_flash_cleanup: brick band changed by {diff} px; "
          f"largest changed cells {top}; "
          f"no stale flash cells vs original L3 reference")


if __name__ == "__main__":
    main()
