#!/usr/bin/env python3
"""Verify side-aware brick collisions: drive the ball into L3's brick
field and check that the ball's bbox sweeps both X *and* Y over time.
With the prior dy-only physics the ball can never traverse a column of
bricks sideways - only top-bottom. With side-aware physics the bbox
should cover both axes meaningfully.

Run: make build/batty-test.img && python3 scripts/exercise_physics.py
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu

OUT = Path('build/exercise_physics')
OUT.mkdir(parents=True, exist_ok=True)

# Boot, advance to L3 (more bricks than L1), release ball, sample frames.
script = ['SLEEP 10',
          'sendkey ret', 'SLEEP 1.5',
          'sendkey ret', 'SLEEP 1.5',
          'sendkey ret', 'SLEEP 1.5',
          'sendkey ret', 'SLEEP 1.5',  # -> L2
          'sendkey ret', 'SLEEP 1.5',  # -> L3
          'sendkey spc', 'SLEEP 0.4']
N = 12
for i in range(N):
    script.append(f'screendump {OUT}/t{i:02d}.ppm')
    script.append('SLEEP 1.0')
script.append('sendkey esc')

run_qemu(Path('build/batty-test.img'), script, OUT/'qemu.log')

# Find bright-white 4x4 cluster in each frame. To filter out HUD digits
# and bat (also bright), restrict to the brick band Y range and require
# at least a 4x4 contiguous white square at center.
def load(p):
    d = p.read_bytes()
    o = 0
    for _ in range(3): o = d.index(b'\n', o) + 1
    return d[o:]

W = 640
def find_ball(img, y_min=72, y_max=260):
    # Look for a 4x4 white square. In 640x400 image, 4 logical px = 8 raster px.
    # Scan rows in y_min..y_max, find 8 consecutive bright-white pixels in 4 stacked rows.
    for y in range(y_min, y_max - 8, 2):
        for x in range(64, W - 64, 2):
            ok = True
            for dy in range(0, 8, 2):
                for dx in range(0, 8, 2):
                    i = ((y+dy) * W + (x+dx)) * 3
                    r, g, b = img[i], img[i+1], img[i+2]
                    if r < 240 or g < 240 or b < 240:
                        ok = False; break
                if not ok: break
            if ok:
                return (x, y)
    return None

print('Sample ball positions per frame (raster coords, 640x400):')
xs, ys = [], []
for i in range(N):
    img = load(Path(f'{OUT}/t{i:02d}.ppm'))
    pos = find_ball(img)
    if pos:
        xs.append(pos[0]); ys.append(pos[1])
        print(f'  t{i:02d}: ball at raster ({pos[0]:3d}, {pos[1]:3d})')
    else:
        print(f'  t{i:02d}: no ball found in brick band (maybe near bat)')

if xs and ys:
    print(f'x range: {min(xs)}..{max(xs)} (span {max(xs)-min(xs)})')
    print(f'y range: {min(ys)}..{max(ys)} (span {max(ys)-min(ys)})')
