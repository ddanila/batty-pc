#!/usr/bin/env python3
"""Headless regression: the ball must BOUNCE off the side / top walls, not
pin against them.

Guards the bounce_wall ($AC75) -> change_direction ($AC40) port in
reflect_obj_dir. A long-standing bug had the X/Y reflect masks SWAPPED and
off by one (flip_x used 0x3F-dir, flip_y used 0x1F-dir), so a ball hitting a
side wall kept its dx pointing INTO the wall: it pinned at x=8 / x=240 and
juggled its dy forever ("balls stuck next to the left/right border"). The
original passes mask $1F for the L/R walls (negate dx) and $3F for the top
(negate dy) -- the same change_direction LAFFC uses for brick faces.

Each case seeds a ball aimed straight at a wall (in the open band below the
bricks and above the bat, so nothing else is hit), runs N frames through the
$BA83 entry, and asserts the ball moved back AWAY from the wall. With the
bug the ball stays pinned within a px or two of the wall.

    make test-wall-bounce
"""
import re
import subprocess
import sys
from pathlib import Path

FLOPPY = 'build/batty-test.img'

# Ball descriptor (22 bytes): sprite_set=02 num=00 x xhi=00 y yhi=00 dir
# speed, then the tail copied from the L3 ball seed (fields the wall path
# doesn't read). We rewrite bytes 2 (x), 4 (y), 6 (dir), 7 (speed).
TAIL = '020CEEF008076C4E020C0000008C'


def ball_seed(x, y, direction, speed):
    return (f'0200{x:02X}00{y:02X}00{direction:02X}{speed:02X}{TAIL}')


def seed_env(ball):
    return (
        'BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 '
        'BATTY_LAFFC=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 '
        'BATTY_REPLAY_BAT_OBJECT=01017400AD000000040DEFAE1C0A74AD040DF0008380 '
        f'BATTY_REPLAY_BALL_OBJECT={ball} BATTY_REPLAY_BALL_STUCK=0')


def ball_at(ball, frame):
    """Build an L3 floppy seeded with `ball`, run `frame` frames, return
    (x, y, dir) of object_ball_1 (or None)."""
    Path(FLOPPY).unlink(missing_ok=True)
    subprocess.run(f'{seed_env(ball)} BATTY_VISUAL_PROBE_FRAMES={frame} '
                   f'make {FLOPPY}', shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, 'scripts/capture_frame_timeline.py',
                    '--floppy', FLOPPY, '--frames', str(frame), '--wait-key',
                    '--out', 'build/tl_wall'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    probe = Path('build/PROBE_wall.txt')
    probe.unlink(missing_ok=True)
    subprocess.run(['mcopy', '-n', '-i', FLOPPY, '::PROBE.TXT', str(probe)],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r'object_ball_1=([0-9A-Fa-f]+)', probe.read_text())
    if not m:
        return None
    b = bytes.fromhex(m.group(1))
    return (b[2], b[4], b[6])


# Each case: a ball seeded in the open band (y=0x90, below bricks / above
# bat) moving straight at a wall at speed 6. By frame 8 it has hit the wall
# (~frame 2-3) and bounced well clear. The wall (left x=8, right x=244) is
# the pin point with the bug; the threshold sits safely between pinned and
# bounced. Measured fixed/buggy: left f8 x=37 / ~8; right f8 x=210 / ~240.
CASES = [
    # name,            x,   y,    dir,  speed, frame, check(x)->ok,  desc
    ('left wall',  20,  0x90, 0x20, 6, 8, lambda x: x >= 24,
     'ball aimed left must bounce back to x>=24 (pins at x=8 with the bug)'),
    ('right wall', 224, 0x90, 0x00, 6, 8, lambda x: x <= 220,
     'ball aimed right must bounce back to x<=220 (pins at x=240 with the bug)'),
]


def main():
    fails = 0
    for name, x, y, direction, speed, frame, ok, desc in CASES:
        got = ball_at(ball_seed(x, y, direction, speed), frame)
        if got is None:
            print(f'  {name}: FAIL (no probe produced)'); fails += 1; continue
        gx, gy, gdir = got
        passed = ok(gx)
        print(f'  {name}: frame {frame} x={gx} y={gy} dir=0x{gdir:02X}  '
              f'[{"PASS" if passed else "FAIL"}] -- {desc}')
        if not passed:
            fails += 1
    if fails == 0:
        print('PASS wall_bounce: ball reflects off the side walls '
              '(change_direction $1F/$3F masks, not pinned)')
    else:
        print(f'FAIL wall_bounce: {fails}/{len(CASES)} walls pin the ball')
    return fails


if __name__ == '__main__':
    sys.exit(main())
