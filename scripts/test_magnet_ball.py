#!/usr/bin/env python3
"""Headless regression: an ON magnet must bend the ball's trajectory; an
OFF magnet must not (known-bugs.md #5 "magnets do not work").

Port of the magnet block at the top of handling_ball (LA27E_0..11): while
a ball overlaps an ON magnet's 15x14 box (slot origin = paint origin +5
on both axes, obj_compare $AC22) its direction rotates +-1/64 per frame;
it releases with a quantized exit dir ((dir+2) & $3C, nudged +-4 off the
pure axes — always a multiple of 4) plus a 2-frame re-capture cooldown
when the magnet switches OFF or the ball leaves the box.

Scenario: L2's single magnet (paint origin $74,$2C -> physics box
x 114..136, y 43..63 for the 8x7 ball) sits in an empty brick pocket
(L2 rows 1..4, cols 6..8 are $C0), so a ball seeded inside the pocket at
(124, 64) aimed straight up (dir $10 — in this encoding $00=right,
$10=up, $20=left, $30=down) reaches the box within a frame and curves
with NO brick interaction (the row-0 bricks above are ~30 frames away).
Two runs differing only in BATTY_REPLAY_MAGNET (forced initial state):

  ON  (mask 1): the dir must have rotated away from $10 by the probe
                frame (delta +1/frame here: quadrant term $FE — dir+$10
                in [$20,$3F] — ball below the centre line, no XOR).
  OFF (mask 0): the ball must fly straight: dir still $10, x unchanged,
                capture state empty.

    make test-magnet-ball
"""
import re
import subprocess
import sys
from pathlib import Path

FLOPPY = 'build/batty-test.img'

# Ball descriptor (22 bytes) — same TAIL as test_wall_bounce (carries
# w_body/h_body = 8x7 at +$0C/+$0D, which the magnet obj_compare reads).
TAIL = '020CEEF008076C4E020C0000008C'

BALL_X = 124
BALL_Y = 0x40          # 64: just below the box (capture needs y <= 63)
BALL_DIR = 0x10        # straight up
BALL_SPEED = 4
PROBE_FRAME = 10


def ball_seed(x, y, direction, speed):
    return (f'0200{x:02X}00{y:02X}00{direction:02X}{speed:02X}{TAIL}')


def run_case(magnet_mask, frame):
    """Boot L2 with the seeded ball and a forced magnet state; return
    (ball_x, ball_y, ball_dir, magnet_state_str) at `frame`."""
    env = (
        'BATTY_LEVEL=2 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 '
        'BATTY_LAFFC=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 '
        'BATTY_REPLAY_BAT_OBJECT=01017400AD000000040DEFAE1C0A74AD040DF0008380 '
        f'BATTY_REPLAY_BALL_OBJECT={ball_seed(BALL_X, BALL_Y, BALL_DIR, BALL_SPEED)} '
        f'BATTY_REPLAY_BALL_STUCK=0 BATTY_REPLAY_MAGNET={magnet_mask:X}')
    Path(FLOPPY).unlink(missing_ok=True)
    subprocess.run(f'{env} BATTY_VISUAL_PROBE_FRAMES={frame} make {FLOPPY}',
                   shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, 'scripts/capture_frame_timeline.py',
                    '--floppy', FLOPPY, '--frames', str(frame), '--wait-key',
                    '--out', 'build/tl_magnet'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    probe = Path('build/PROBE_magnet.txt')
    probe.unlink(missing_ok=True)
    subprocess.run(['mcopy', '-n', '-i', FLOPPY, '::PROBE.TXT', str(probe)],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    text = probe.read_text()
    m = re.search(r'object_ball_1=([0-9A-Fa-f]+)', text)
    s = re.search(r'magnet_state=(\S+)', text)
    if not m:
        return None
    b = bytes.fromhex(m.group(1))
    return (b[2], b[4], b[6], s.group(1) if s else '')


def main():
    fails = 0

    on = run_case(1, PROBE_FRAME)
    if on is None:
        print('  magnet ON: FAIL (no probe produced)')
        fails += 1
    else:
        x, y, d, st = on
        # Captured: delta -1/frame from the first overlap frame, so the
        # dir must have left $30 (still captured: $30-9..$30-1; released:
        # a multiple of 4 != $30). x drifts left with the curve.
        ok = (d != BALL_DIR)
        print(f'  magnet ON : frame {PROBE_FRAME} x={x} y={y} dir=0x{d:02X} '
              f'state={st}  [{"PASS" if ok else "FAIL"}] -- dir must rotate '
              f'away from 0x{BALL_DIR:02X} while in the ON box')
        if not ok:
            fails += 1

    off = run_case(0, PROBE_FRAME)
    if off is None:
        print('  magnet OFF: FAIL (no probe produced)')
        fails += 1
    else:
        x, y, d, st = off
        ok = (d == BALL_DIR and x == BALL_X)
        print(f'  magnet OFF: frame {PROBE_FRAME} x={x} y={y} dir=0x{d:02X} '
              f'state={st}  [{"PASS" if ok else "FAIL"}] -- OFF magnet must '
              f'not touch the trajectory (dir 0x{BALL_DIR:02X}, x={BALL_X})')
        if not ok:
            fails += 1

    if fails == 0:
        print('PASS magnet_ball: ON magnet curves the ball (LA27E_0..11), '
              'OFF magnet leaves it alone')
    else:
        print(f'FAIL magnet_ball: {fails}/2 cases')
    return fails


if __name__ == '__main__':
    sys.exit(main())
