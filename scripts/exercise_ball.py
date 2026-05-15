#!/usr/bin/env python3
"""Boot the test floppy, advance to L1, press SPACE to release the
ball, wait for it to bounce a few times, screendump.

Run: make build/batty-test.img && python3 scripts/exercise_ball.py
Output: build/exercise_ball/{after_release,after_bounces}.ppm
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu

OUT = Path('build/exercise_ball')
OUT.mkdir(parents=True, exist_ok=True)

script = [
    'SLEEP 10',                      # boot to title
    'sendkey ret', 'SLEEP 1.5',      # title -> menu
    'sendkey ret', 'SLEEP 1.5',      # menu -> hiscore
    'sendkey ret', 'SLEEP 1.5',      # hiscore -> L1
    f'screendump {OUT}/initial.ppm', 'SLEEP 0.3',
    'sendkey spc', 'SLEEP 0.4',      # release ball
    f'screendump {OUT}/after_release.ppm', 'SLEEP 0.3',
    'SLEEP 2.0',                     # let ball bounce
    f'screendump {OUT}/after_bounces.ppm', 'SLEEP 0.3',
    'SLEEP 8.0',                     # plenty of time to hit bricks
    f'screendump {OUT}/after_8s.ppm', 'SLEEP 0.3',
    'sendkey left', 'sendkey left', 'sendkey left', 'SLEEP 0.5',
    f'screendump {OUT}/after_bat_left.ppm', 'SLEEP 0.3',
    'sendkey esc',
]
run_qemu(Path('build/batty-test.img'), script, OUT/'qemu.log')
print('captures in', OUT)
