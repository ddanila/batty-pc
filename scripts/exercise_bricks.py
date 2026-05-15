#!/usr/bin/env python3
"""Exercise the brick-destruction wiring. Boot, advance past L1+L2
to L3 (more brick coverage), release ball, run for ~10 sec, screendump
brick band at start vs end. Any per-pixel diff in the brick band proves
brick(s) were destroyed."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu

OUT = Path('build/exercise_bricks')
OUT.mkdir(parents=True, exist_ok=True)

script = [
    'SLEEP 10',
    'sendkey ret', 'SLEEP 1.5',   # title -> menu
    'sendkey ret', 'SLEEP 1.5',   # menu -> hiscore
    'sendkey ret', 'SLEEP 1.5',   # hiscore -> L1
    'sendkey ret', 'SLEEP 1.5',   # L1 -> L2
    'sendkey ret', 'SLEEP 1.5',   # L2 -> L3
    f'screendump {OUT}/l3_initial.ppm', 'SLEEP 0.3',
    'sendkey spc',                # release ball
    'SLEEP 12',
    f'screendump {OUT}/l3_after.ppm', 'SLEEP 0.3',
    'sendkey esc',
]
run_qemu(Path('build/batty-test.img'), script, OUT/'qemu.log')

def load(p):
    d = p.read_bytes()
    o = 0
    for _ in range(3): o = d.index(b'\n', o)+1
    return d[o:]

W = 640
def diff_brick_band(a, b):
    # Playfield brick band y=32..127 -> screen y=36..131 -> qemu y=72..264
    d = 0
    for y in range(72, 264):
        for x in range(2*32 + 16, 2*256 - 16):
            i = (y*W + x) * 3
            if a[i:i+3] != b[i:i+3]:
                d += 1
    return d

a = load(OUT/'l3_initial.ppm')
b = load(OUT/'l3_after.ppm')
print(f'L3 brick band diff after 12s of play: {diff_brick_band(a, b)} px (excluding side frame)')
