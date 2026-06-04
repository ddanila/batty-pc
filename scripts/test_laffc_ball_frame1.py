#!/usr/bin/env python3
"""Headless regression: byte-exact LAFFC ball trajectory on L3.

Locks in the parity milestone (ball motion + brick collision) without
ZEsarUX. The L3-seeded LAFFC port, stepped N game frames from the aligned
$BA83 entry, must produce the exact ball object the Spectrum does at each
checkpoint — probed via
`scripts/capture_frame_timeline_original.py --probe-ball 0x9AD0`:

    frame 1 : x=0x69 xf=0x09 y=0x41 yf=0x48 dir=0x21
    frame 5 : x=0x70 xf=0xF7 y=0x40 yf=0x28 dir=0x21
    frame 10: x=0x6B xf=0x12 y=0x3E yf=0xC0 dir=0x3F
    frame 40: x=0x71 xf=0x00 y=0x36 yf=0x50 dir=0x3F

frame 1 covers the exact-motion chain (dir_to_dxdy LAD69 cross, q8.8
fraction, the vertical-bounce + fraction-preserving snap). frames 5/10/40
cover repeated collisions including the horizontal/side bounce and the
LAFFC_5-6 down/down-right straddle — any regression flips a byte.

This script builds its own L3-seeded LAFFC floppy per checkpoint, runs to
that frame (WAIT_KEY pause -> N frames -> halt with PROBE.TXT written),
extracts object_ball_1, and asserts x/xf/y/yf/dir. Exit 0 = all PASS.

    make test-laffc-ball-frame1
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

FLOPPY = 'build/batty-test.img'
SEED = ('BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 BATTY_LAFFC=1 '
        'BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 '
        'BATTY_REPLAY_BAT_OBJECT=01017400AD000000040DEFAE1C0A74AD040DF0008380 '
        'BATTY_REPLAY_BALL_OBJECT=02006C004E001F03020CEEF008076C4E020C0000008C '
        'BATTY_REPLAY_BALL_STUCK=0')

# frame -> (x, xf, y, yf, dir) from the original probe. Spans L3's first
# ~150 frames (dozens of bounces, many cell configs) -- all byte-exact.
EXPECT = {
    1:   (0x69, 0x09, 0x41, 0x48, 0x21),
    5:   (0x70, 0xF7, 0x40, 0x28, 0x21),
    10:  (0x6B, 0x12, 0x3E, 0xC0, 0x3F),
    40:  (0x71, 0x00, 0x36, 0x50, 0x3F),
    60:  (117,  238,  48,   176,  0x21),
    80:  (141,  166,  43,   16,   0x21),
    100: (124,  220,  37,   112,  0x3F),
    150: (17,   32,   23,   96,   0x21),
}


def ball_at(frame):
    Path(FLOPPY).unlink(missing_ok=True)
    subprocess.run(f'{SEED} BATTY_VISUAL_PROBE_FRAMES={frame} make {FLOPPY}',
                   shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, 'scripts/capture_frame_timeline.py',
                    '--floppy', FLOPPY, '--frames', str(frame), '--wait-key',
                    '--out', 'build/tl_laffc_ball'], stdout=subprocess.DEVNULL)
    probe = Path('build/PROBE_laffc.txt')
    subprocess.run(['mcopy', '-n', '-i', FLOPPY, '::PROBE.TXT', str(probe)],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return None
    m = re.search(r'object_ball_1=([0-9A-Fa-f]+)', probe.read_text())
    if not m:
        return None
    b = bytes.fromhex(m.group(1))
    return (b[2], b[3], b[4], b[5], b[6])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--frames', default='1,5,40,100,150',
                    help='subset of the checkpoint frames to verify')
    args = ap.parse_args()
    frames = [int(x) for x in args.frames.split(',') if x.strip()]

    fails = 0
    for f in frames:
        exp = EXPECT.get(f)
        if exp is None:
            print(f'  frame {f}: no expected value on record, skipping'); continue
        got = ball_at(f)
        if got is None:
            print(f'  frame {f}: FAIL (no probe produced)'); fails += 1; continue
        ok = got == exp
        fmt = lambda t: ' '.join(f'{n}=0x{v:02X}' for n, v in zip('x xf y yf dir'.split(), t))
        print(f'  frame {f}: {fmt(got)}  [{"PASS" if ok else "FAIL exp " + fmt(exp)}]')
        if not ok:
            fails += 1

    if fails == 0:
        print(f'PASS laffc_ball: byte-exact vs Spectrum at frames {frames} '
              f'(motion + collision incl. side bounce + LAFFC_5-6 straddle)')
    else:
        print(f'FAIL laffc_ball: {fails}/{len(frames)} checkpoints diverged')
    return fails


if __name__ == '__main__':
    sys.exit(main())
