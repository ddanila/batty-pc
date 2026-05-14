#!/usr/bin/env python3
"""Headless visual regression test.

Boots build/batty.img in QEMU with `-display none -monitor stdio`,
captures the framebuffer at each checkpoint, decodes it back into
palette-index space, and diffs against the ZEsarUX snapshot decoded
through our ZX palette. Pixel-identical => PASS.

Returns exit code = number of failed checkpoints.
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

# 6-bit DAC values written by src/main.c (ZX_LO=56, ZX_HI=63).
# QEMU's mode-13h output scales them by plain shift-2 (no LSB replication):
# DAC 56 -> 0xE0 (224), DAC 63 -> 0xFF (255). DAC 0 -> 0.
ZX_LO = 56
ZX_HI = 63
ZX_LO_8 = 224
ZX_HI_8 = 255
SCREEN_W, SCREEN_H = 320, 200
PLAYFIELD_W, PLAYFIELD_H = 256, 192
BORDER_X, BORDER_Y = 32, 4


def build_palette_rgb():
    """16 ZX palette RGB triples in the 8-bit values QEMU emits."""
    out = []
    for level in (ZX_LO_8, ZX_HI_8):
        for ink in range(8):
            r = level if (ink & 2) else 0
            g = level if (ink & 4) else 0
            b = level if (ink & 1) else 0
            out.append((r, g, b))
    return out


PALETTE_RGB = build_palette_rgb()
PAL_LOOKUP  = {rgb: i for i, rgb in enumerate(PALETTE_RGB)}


def rgb_to_index(rgb):
    if rgb in PAL_LOOKUP:
        return PAL_LOOKUP[rgb]
    return min(range(16),
               key=lambda i: sum((a-b)**2 for a, b in zip(rgb, PALETTE_RGB[i])))


def parse_ppm(path: Path):
    """Parse a P6 binary PPM. Returns (w, h, raw_rgb_bytes)."""
    data = path.read_bytes()
    # Header is 3 ASCII tokens separated by whitespace (comments start with '#').
    i = 0
    def next_token():
        nonlocal i
        while i < len(data) and data[i:i+1] in b' \t\r\n':
            i += 1
        if i < len(data) and data[i:i+1] == b'#':
            while i < len(data) and data[i:i+1] != b'\n':
                i += 1
            return next_token()
        start = i
        while i < len(data) and data[i:i+1] not in b' \t\r\n':
            i += 1
        return data[start:i]
    magic  = next_token()
    w      = int(next_token())
    h      = int(next_token())
    maxval = int(next_token())
    if magic != b'P6':   raise ValueError(f'expected P6, got {magic!r}')
    if maxval != 255:    raise ValueError(f'expected maxval 255, got {maxval}')
    i += 1   # skip single-byte separator after maxval
    return w, h, data[i:]


def ppm_inner_to_indices(path: Path):
    """Extract the 256x192 playfield region from QEMU's PPM, map to palette.
    QEMU outputs mode 13h scaled 2x to 640x400 for aspect correction —
    detect the scale and sample one pixel per VGA pixel cell."""
    w, h, raw = parse_ppm(path)
    if (w, h) == (SCREEN_W, SCREEN_H):
        scale = 1
    elif (w, h) == (SCREEN_W * 2, SCREEN_H * 2):
        scale = 2
    else:
        raise ValueError(f'unexpected PPM size {w}x{h}; expected 320x200 or 640x400')
    out = bytearray(PLAYFIELD_W * PLAYFIELD_H)
    for y in range(PLAYFIELD_H):
        py = (BORDER_Y + y) * scale
        for x in range(PLAYFIELD_W):
            px = (BORDER_X + x) * scale
            off = (py * w + px) * 3
            out[y * PLAYFIELD_W + x] = rgb_to_index((raw[off], raw[off+1], raw[off+2]))
    return bytes(out)


def expected_from_scr(scr_path: Path):
    sys.path.insert(0, str(Path(__file__).parent))
    from extract_scr import decode
    return decode(scr_path.read_bytes())


def run_qemu(floppy: Path, script: list, log_path: Path):
    """Drive QEMU via -monitor stdio. `script` is a list of either
    'SLEEP <secs>' or raw monitor commands."""
    log = log_path.open('wb')
    proc = subprocess.Popen([
        'qemu-system-i386',
        '-drive', f'if=floppy,format=raw,file={floppy}',
        '-boot', 'a',
        '-m', '4',
        '-display', 'none',
        '-monitor', 'stdio',
        '-no-reboot',
    ], stdin=subprocess.PIPE, stdout=log, stderr=log)
    try:
        for step in script:
            if step.startswith('SLEEP '):
                time.sleep(float(step.split()[1]))
            else:
                proc.stdin.write((step + '\n').encode())
                proc.stdin.flush()
                time.sleep(0.2)
        proc.stdin.write(b'quit\n')
        proc.stdin.flush()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
    finally:
        log.close()


def make_diff_png(actual_idx: bytes, expected_idx: bytes, out_path: Path):
    """Write a side-by-side diff (red where pixels disagree)."""
    try:
        from PIL import Image
    except ImportError:
        return
    img = Image.new('RGB', (PLAYFIELD_W, PLAYFIELD_H))
    px = img.load()
    for y in range(PLAYFIELD_H):
        for x in range(PLAYFIELD_W):
            i = y * PLAYFIELD_W + x
            if actual_idx[i] == expected_idx[i]:
                # Render expected in grey for context.
                v = 64 if expected_idx[i] != 0 else 0
                px[x, y] = (v, v, v)
            else:
                px[x, y] = (255, 0, 0)
    img.save(out_path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--floppy', default='build/batty.img')
    ap.add_argument('--out', default='build/test_visual')
    ap.add_argument('--boot-wait', type=float, default=10.0)
    ap.add_argument('--state-wait', type=float, default=1.5)
    args = ap.parse_args()
    floppy = Path(args.floppy)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    SNAP_HISCORE = Path('build/snapshots/20260513T202038Z/screen.scr')
    SNAP_MENU    = Path('build/snapshots/20260513T202041Z/screen.scr')
    TITLE_SCR    = Path('original/Batty.scr')
    GT_LEVEL1    = Path('build/level_gt/level_01.scr')
    # Per checkpoint: (label, expected_scr, assert_match, roi)
    # roi=None  -> diff the full 256x192 playfield
    # roi=(x0, y0, x1, y1) -> diff only that sub-rectangle in playfield coords
    # `assert_match=False` => captured, diff-reported, but not failing.
    # state4_level1 ROI covers the brick zone (8..248, 16..112) plus the
    # bat strip (8..248, 167..183). We diff the union by using a single
    # bounding box that includes both bands; the gap rows (113..166) are
    # plain hex bg which mostly matches anyway so the metric stays
    # meaningful.
    BRICK_ROI = (8, 16, 8 + 240, 184)
    checkpoints = [
        ('state1_title',    TITLE_SCR,    True,  None),
        ('state2_menu',     SNAP_MENU,    False, None),
        ('state3_hiscore',  SNAP_HISCORE, True,  None),
        ('state4_level1',   GT_LEVEL1,    False, BRICK_ROI),
    ]

    script = [f'SLEEP {args.boot_wait}']
    for i, cp in enumerate(checkpoints):
        label = cp[0]
        script.append(f'screendump {out/label}.ppm')
        script.append('SLEEP 0.3')
        if i < len(checkpoints) - 1:
            script.append('sendkey ret')
            script.append(f'SLEEP {args.state_wait}')
    script.append('sendkey esc')

    print(f'booting {floppy} headless (boot wait {args.boot_wait}s)...')
    run_qemu(floppy, script, out / 'qemu.log')

    failed = 0
    for label, expected_scr, assert_match, roi in checkpoints:
        ppm_path = out / f'{label}.ppm'
        if not ppm_path.exists():
            print(f'  FAIL {label}: no PPM produced'); failed += 1; continue
        actual   = ppm_inner_to_indices(ppm_path)
        expected = expected_from_scr(expected_scr)
        if roi is None:
            diff = sum(1 for a, e in zip(actual, expected)
                       if PALETTE_RGB[a] != PALETTE_RGB[e])
            total = PLAYFIELD_W * PLAYFIELD_H
        else:
            x0, y0, x1, y1 = roi
            diff = 0
            total = (x1 - x0) * (y1 - y0)
            for y in range(y0, y1):
                row = y * PLAYFIELD_W
                for x in range(x0, x1):
                    if PALETTE_RGB[actual[row + x]] != PALETTE_RGB[expected[row + x]]:
                        diff += 1
        pct = 100.0 * diff / total
        if diff == 0:
            print(f'  PASS {label}: pixel-identical ({total} px)'
                  + (f' [roi {roi}]' if roi else ''))
        else:
            make_diff_png(actual, expected, out / f'{label}_diff.png')
            tag = 'FAIL' if assert_match else 'INFO'
            roi_tag = f' [roi {roi}]' if roi else ''
            print(f'  {tag} {label}: {diff}/{total} px differ ({pct:.2f}%){roi_tag}')
            print(f'        diff -> {out}/{label}_diff.png')
            if assert_match:
                failed += 1

    sys.exit(failed)


if __name__ == '__main__':
    main()
