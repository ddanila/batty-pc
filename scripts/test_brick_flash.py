#!/usr/bin/env python3
"""Regression check for brick-destruction flash cleanup.

Drives the DOS build into L3, releases the ball, waits long enough for
at least one brick interaction, then verifies that no solid bright-white
flash rectangle remains in the brick grid after the flash animation
should have restored the level background.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import PALETTE_RGB, PLAYFIELD_W, ppm_inner_to_indices, run_qemu

OUT = Path("build/test_brick_flash")
FLOPPY = Path("build/batty-test.img")
ROWS, COLS = 12, 15
BRICK_X0, BRICK_Y0 = 8, 32
BRICK_W, BRICK_H = 16, 8
BRIGHT_WHITE = (255, 255, 255)


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


def stale_flash_cells(idx):
    cells = []
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
            if white >= 96:
                cells.append((r, c, white))
    return cells


def main():
    capture()
    initial = ppm_inner_to_indices(OUT / "l3_initial.ppm")
    after = ppm_inner_to_indices(OUT / "l3_after.ppm")
    diff = brick_band_diff(initial, after)
    if diff == 0:
        raise SystemExit("FAIL: L3 brick band did not change; test did not exercise brick destruction")
    stale = stale_flash_cells(after)
    if stale:
        detail = ", ".join(f"r{r}c{c}:{white}" for r, c, white in stale)
        raise SystemExit(f"FAIL: stale bright-white brick flash cells remain: {detail}")
    print(f"PASS brick_flash_cleanup: brick band changed by {diff} px; no stale flash cells")


if __name__ == "__main__":
    main()
