#!/usr/bin/env python3
"""Drive the game loop: lose all 3 lives, screendump the GAME OVER
screen. After each miss the ball needs a fresh SPACE to relaunch.

Strategy: move the bat to the far left (so ball drops past on the
right), release ball, wait for the drop, repeat 3 times.

Run: make build/batty-test.img && python3 scripts/exercise_gameloop.py
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu

OUT = Path('build/exercise_gameloop')
OUT.mkdir(parents=True, exist_ok=True)

# Each LEFT moves the bat 4 px. To move from x=112 to x=8 we need
# 26 LEFTs.
left_run = ['sendkey left'] * 30

# Ball trajectory at default angle takes ~8.5 sec to miss with bat
# parked left (it always heads up-right -> right wall -> top -> down -> miss).
def lose_one_life():
    return ['sendkey spc', 'SLEEP 9.0']

script = ['SLEEP 10',
          'sendkey ret', 'SLEEP 1.5',
          'sendkey ret', 'SLEEP 1.5',
          'sendkey ret', 'SLEEP 1.5']  # title -> menu -> hiscore -> L1
script += left_run + ['SLEEP 0.5']
script += lose_one_life() + [f'screendump {OUT}/after_life1.ppm', 'SLEEP 0.3']
script += lose_one_life() + [f'screendump {OUT}/after_life2.ppm', 'SLEEP 0.3']
script += lose_one_life() + [f'screendump {OUT}/after_life3.ppm', 'SLEEP 0.3']
script += ['SLEEP 0.5', f'screendump {OUT}/game_over.ppm', 'SLEEP 0.3']
script += ['sendkey esc']

run_qemu(Path('build/batty-test.img'), script, OUT/'qemu.log')

def load(p):
    d = p.read_bytes()
    o = 0
    for _ in range(3): o = d.index(b'\n', o) + 1
    return d[o:]

W = 640
# Game-over screen is mostly black. Count near-black pixels in playfield.
def black_fraction(img):
    n = 0; t = 0
    for y in range(20, 380):
        for x in range(60, 580):
            i = (y * W + x) * 3
            r, g, b = img[i], img[i+1], img[i+2]
            t += 1
            if r < 30 and g < 30 and b < 30:
                n += 1
    return n / t

go = load(OUT/'game_over.ppm')
print(f'game_over.ppm: {black_fraction(go)*100:.1f}% near-black pixels '
      f'(GAME OVER screen blanks to color 0)')
