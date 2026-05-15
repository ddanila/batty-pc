#!/usr/bin/env python3
"""Verify the PAUSED banner appears on P keypress. Boots to L1, sends
SPACE, waits 1 sec, sends P, screendumps. Expect bright white pixels
at the banner position.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu

OUT = Path('build/exercise_pause')
OUT.mkdir(parents=True, exist_ok=True)

script = ['SLEEP 10',
          'sendkey ret', 'SLEEP 1.5',
          'sendkey ret', 'SLEEP 1.5',
          'sendkey ret', 'SLEEP 1.5',
          'sendkey spc', 'SLEEP 1.0',
          f'screendump {OUT}/playing.ppm', 'SLEEP 0.3',
          'sendkey p',   'SLEEP 0.5',
          f'screendump {OUT}/paused.ppm', 'SLEEP 0.3',
          'sendkey p',   'SLEEP 1.0',
          f'screendump {OUT}/resumed.ppm', 'SLEEP 0.3',
          'sendkey esc']
run_qemu(Path('build/batty-test.img'), script, OUT/'qemu.log')

def load(p):
    d = p.read_bytes()
    o = 0
    for _ in range(3): o = d.index(b'\n', o)+1
    return d[o:]
W = 640
def count_white_in_band(img, x0, y0, x1, y1):
    n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            i = (y*W + x)*3
            r, g, b = img[i], img[i+1], img[i+2]
            if r > 240 and g > 240 and b > 240:
                n += 1
    return n

# PAUSED banner at logical x=136, y=94. 6 chars * 8 = 48 px wide x 6 px tall.
# Raster: x=272..368, y=188..200
for name in ['playing', 'paused', 'resumed']:
    img = load(Path(f'{OUT}/{name}.ppm'))
    n = count_white_in_band(img, 272, 188, 368, 200)
    print(f'{name:9s}  bright white at banner position: {n}')
