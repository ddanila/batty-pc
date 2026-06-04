#!/usr/bin/env python3
"""Headless regression: byte-exact LAFFC ball state at L3 frame 1.

Locks in the parity milestone (ball motion + brick collision) without
needing ZEsarUX. The L3-seeded LAFFC port, stepped one game frame from
the aligned $BA83 entry, must produce the exact ball object the Spectrum
does at the same point — probed via
`scripts/capture_frame_timeline_original.py --probe-ball 0x9AD0`:

    x=105 (0x69)  xf=9   y=65 (0x41)  yf=72 (0x48)  dir=0x21

That covers the whole exact-motion chain: dir_to_dxdy (LAD69 X/Y cross),
the q8.8 fraction, the LAFFC up-bounce cell/axis, change_direction, and
the fraction-preserving cell-edge snap. Any regression in those flips one
of these bytes.

Drives the port via capture_frame_timeline (WAIT_KEY pause -> 1 frame ->
halt with PROBE.TXT written), extracts PROBE.TXT from the floppy, and
asserts object_ball_1's x/xf/y/yf/dir. Exit 0 = PASS.

The Makefile target builds the matching floppy:
    make test-laffc-ball-frame1
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

# object_ball_1 layout: +2 x, +3 x-frac, +4 y, +5 y-frac, +6 dir.
EXPECT = {'x': 0x69, 'xf': 0x09, 'y': 0x41, 'yf': 0x48, 'dir': 0x21}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--floppy', default='build/batty-test.img')
    args = ap.parse_args()
    floppy = Path(args.floppy)

    # Drive the port: pause at $BA83, step to frame 1, halt (PROBE.TXT written).
    rc = subprocess.run([sys.executable, str(Path(__file__).parent / 'capture_frame_timeline.py'),
                         '--floppy', str(floppy), '--frames', '1', '--wait-key',
                         '--out', 'build/tl_laffc_ball'],
                        stdout=subprocess.DEVNULL).returncode
    if rc != 0:
        print('FAIL: capture run errored'); return 1

    # Extract PROBE.TXT from the DOS floppy.
    probe = Path('build/PROBE_laffc.txt')
    subprocess.run(['mcopy', '-n', '-i', str(floppy), '::PROBE.TXT', str(probe)],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        print('FAIL: PROBE.TXT not produced (is BATTY_REPLAY_PROBE=1 set on the floppy?)')
        return 1
    m = re.search(r'object_ball_1=([0-9A-Fa-f]+)', probe.read_text())
    if not m:
        print('FAIL: object_ball_1 not in PROBE.TXT'); return 1
    b = bytes.fromhex(m.group(1))
    got = {'x': b[2], 'xf': b[3], 'y': b[4], 'yf': b[5], 'dir': b[6]}

    ok = got == EXPECT
    print('  ball@frame1:', ' '.join(f'{k}=0x{got[k]:02X}' for k in EXPECT),
          '(expected', ' '.join(f'{k}=0x{EXPECT[k]:02X}' for k in EXPECT) + ')')
    if ok:
        print('PASS laffc_ball_frame1: byte-exact vs Spectrum (motion + collision)')
        return 0
    print('FAIL laffc_ball_frame1: ball state diverged from the Spectrum')
    return 1


if __name__ == '__main__':
    sys.exit(main())
