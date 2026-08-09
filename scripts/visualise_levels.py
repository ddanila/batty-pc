#!/usr/bin/env python3
"""Capture all 15 levels and grid them beside the GT captures — A TOOL.

NOT A GATE, and renamed from sweep_levels.py on 2026-08-10 to stop it
being read as one. It has no failure path at all: it prints per-level
diff counts and writes a composite PNG for a human to look at, and
always exits 0.

`make test-levels-sweep` is the GATE, and it runs
scripts/test_levels_sweep.py — a different file whose name was one
character away. Mid-session I read this script, saw it could never fail,
and was about to report that the levels gate was a visualiser. It is
not; test_levels_sweep.py runs test-visual's state4_level1 per level and
fails properly. The names were close enough to draw the wrong
conclusion from the right observation.

Use this when a level looks wrong and you want to SEE the difference;
use the gate when you want to know whether anything broke.

Boots build/batty-test.img (BATTYALL=1 so every transition is keypress-
driven), advances through TITLE -> MENU -> HISCORE -> ST_LEVEL with
ENTER keys, then sends one ENTER per level to cycle L1..L15, dumping
the framebuffer between each.

Produces build/sweep/level_NN.ppm + .png and a final composite grid:
build/sweep/sweep_grid.png (3 cols of [ours | GT | red-diff] x 15 rows).
"""
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import (parse_ppm, ppm_inner_to_indices,
                         expected_from_scr, PALETTE_RGB)

OUT = Path('build/sweep')
N_LEVELS = 15
FLOPPY  = Path('build/batty-test.img')
GT_DIR  = Path('build/level_gt')
W, H = 256, 192


def drive_qemu():
    OUT.mkdir(parents=True, exist_ok=True)
    proc = subprocess.Popen([
        'qemu-system-i386',
        '-drive', f'if=floppy,format=raw,file={FLOPPY}',
        '-boot', 'a', '-m', '4',
        '-display', 'none', '-monitor', 'stdio',
        '-no-reboot',
    ], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    def send(line, wait=0.3):
        proc.stdin.write((line + '\n').encode()); proc.stdin.flush()
        time.sleep(wait)
    try:
        time.sleep(10.0)                              # boot
        send('sendkey ret', 1.5)                      # title -> menu
        send('sendkey ret', 1.5)                      # menu  -> hiscore
        send('sendkey ret', 1.5)                      # hiscore -> ST_LEVEL (= L1)
        for n in range(1, N_LEVELS + 1):
            send(f'screendump {OUT/f"level_{n:02d}.ppm"}', 0.5)
            if n < N_LEVELS:
                send('sendkey ret', 1.5)              # advance to next level
        send('sendkey esc', 0.2)
        send('quit', 0.0)
    finally:
        try: proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill(); proc.wait()


def ppm_to_image(path):
    from PIL import Image
    w, h, raw = parse_ppm(path)
    return Image.frombytes('RGB', (w, h), bytes(raw))


def diff_count(ours_path, gt_scr):
    ours = ppm_inner_to_indices(ours_path)
    gt   = expected_from_scr(gt_scr)
    return sum(1 for a, e in zip(ours, gt)
               if PALETTE_RGB[a] != PALETTE_RGB[e])


def build_diff_image(ours_path, gt_scr):
    from PIL import Image
    ours = ppm_inner_to_indices(ours_path)
    gt   = expected_from_scr(gt_scr)
    img = Image.new('RGB', (W, H), (0, 0, 0))
    px = img.load()
    for y in range(H):
        for x in range(W):
            i = y * W + x
            if PALETTE_RGB[ours[i]] != PALETTE_RGB[gt[i]]:
                px[x, y] = (255, 0, 0)
            else:
                v = 30 if gt[i] not in (0, 8) else 0
                px[x, y] = (v, v, v)
    return img


def main():
    from PIL import Image, ImageDraw, ImageFont
    drive_qemu()

    # Sanity check that we got 15 PPMs.
    missing = [n for n in range(1, N_LEVELS + 1)
               if not (OUT / f'level_{n:02d}.ppm').exists()]
    if missing:
        print(f'WARN: missing captures for {missing}')

    # Per-level diff numbers + composite grid.
    GAP = 6
    CELL_W = W
    CELL_H = H
    LABEL_W = 80
    grid_w = LABEL_W + GAP + 3 * (CELL_W + GAP)
    grid_h = N_LEVELS * (CELL_H + GAP) + GAP
    grid = Image.new('RGB', (grid_w, grid_h), (24, 24, 32))
    d = ImageDraw.Draw(grid)
    try: font = ImageFont.load_default()
    except Exception: font = None

    print()
    print(f'{"level":>6} {"diff_px":>10} {"of":>10} {"pct":>6}')
    for n in range(1, N_LEVELS + 1):
        ppm = OUT / f'level_{n:02d}.ppm'
        scr = GT_DIR / f'level_{n:02d}.scr'
        y0 = GAP + (n - 1) * (CELL_H + GAP)
        if not ppm.exists():
            print(f'  L{n:2d}: no capture')
            continue
        # Save PNG
        our_img = ppm_to_image(ppm)
        # Crop the playfield 256x192 out of the 640x400 (2x scaled) PPM
        # Just resize is wrong since the PPM is a full 640x400; we need the
        # 256x192 inner region.
        # PPM is 640x400, playfield offset is BORDER_X*2 .. BORDER_Y*2.
        sw, sh = our_img.size
        if (sw, sh) == (W * 2, H * 2):
            our_img = our_img.crop((64, 8, 64 + W*2, 8 + H*2)).resize((W, H), Image.BOX)
        our_img.save(OUT / f'level_{n:02d}.png')

        # GT image
        gt_idx = expected_from_scr(scr)
        gt_img = Image.new('RGB', (W, H))
        gpx = gt_img.load()
        for y in range(H):
            for x in range(W):
                gpx[x, y] = PALETTE_RGB[gt_idx[y*W + x]]

        # Diff image
        diff_img = build_diff_image(ppm, scr)
        diff_n = diff_count(ppm, scr)
        pct = 100.0 * diff_n / (W * H)
        print(f'  L{n:2d}: {diff_n:8d} / {W*H} ({pct:5.2f}%)')

        # Paste into grid
        d.text((4, y0 + 4), f'L{n:02d}\n{pct:.2f}%', fill=(220, 220, 220), font=font)
        grid.paste(our_img,  (LABEL_W + GAP,                       y0))
        grid.paste(gt_img,   (LABEL_W + GAP + CELL_W + GAP,         y0))
        grid.paste(diff_img, (LABEL_W + GAP + 2 * (CELL_W + GAP),   y0))

    # Header strip
    d.text((LABEL_W + GAP + CELL_W//2 - 20, 1), 'ours',  fill=(180, 220, 255), font=font)
    d.text((LABEL_W + GAP + CELL_W + GAP + CELL_W//2 - 10, 1), 'GT', fill=(180, 220, 255), font=font)
    d.text((LABEL_W + GAP + 2*(CELL_W + GAP) + CELL_W//2 - 20, 1), 'diff', fill=(255, 180, 180), font=font)

    grid.save(OUT / 'sweep_grid.png')
    print(f'\nwrote {OUT}/sweep_grid.png')


if __name__ == '__main__':
    main()
