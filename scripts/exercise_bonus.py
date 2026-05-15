#!/usr/bin/env python3
"""Capture frames every 0.5 sec over 15 sec of L3 play and look for red
or magenta pixels in the bonus-fall band (= bonus falling). Reports
any frame that contains them.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu

OUT = Path('build/exercise_bonus')
OUT.mkdir(parents=True, exist_ok=True)

script = ['SLEEP 10']
for _ in range(5):
    script += ['sendkey ret', 'SLEEP 1.5']     # title->menu->hiscore->L1->L2->L3 + buffer
script.append('sendkey spc')
N = 30
for i in range(N):
    script.append(f'screendump {OUT}/t{i:02d}.ppm')
    script.append('SLEEP 0.5')
script.append('sendkey esc')

run_qemu(Path('build/batty-test.img'), script, OUT/'qemu.log')

def load(p):
    d = p.read_bytes()
    o = 0
    for _ in range(3): o = d.index(b'\n', o)+1
    return d[o:]
W = 640

def count_bonus(img):
    red = magenta = 0
    for y in range(262, 342):
        for x in range(80, 560):
            i = (y*W + x)*3
            r, g, b = img[i], img[i+1], img[i+2]
            if r > 180 and g < 80 and b < 80: red += 1
            elif r > 180 and g < 80 and b > 180: magenta += 1
    return red, magenta

hit_count = 0
for i in range(N):
    img = load(Path(f'{OUT}/t{i:02d}.ppm'))
    r, m = count_bonus(img)
    if r or m:
        print(f't{i:02d}: red={r:3d} magenta={m:3d}')
        hit_count += 1
print(f'\nframes with a falling bonus: {hit_count}/{N}')
