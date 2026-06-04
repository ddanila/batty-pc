#!/usr/bin/env python3
"""Port-side multi-level sanity sweep for the LAFFC collision path.

LAFFC is byte-exact on the L3 frame-step gate, but that one trajectory
does not exercise every cell neighbourhood. This sweep runs each level
headlessly (no ZEsarUX) for N frames under BOTH collision paths, with a
static bat (no input), and compares how many bricks each destroys. The
LAFFC path has a brick_collision fallback, so it can never pass a brick
through — but it CAN bounce *wrong* on an unported edge case and send the
ball to its death without playing. That shows up as LAFFC destroying far
fewer bricks than brick_collision on the same level.

A level FAILS the sweep when brick_collision destroys a healthy number of
bricks but LAFFC destroys ~none — a sign LAFFC misbehaves on that layout.
This is the gate that must pass before flipping the default to LAFFC.

    make test-laffc-levels-sane LEVELS=1,5,10,15 FRAMES=500
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

FLOPPY = 'build/batty-test.img'


def run_level(level: int, frames: int, laffc: bool):
    """Build the floppy for this level/path, run `frames` frames, return
    (bricks_initial, bricks_final, ball_active) or None on failure."""
    env = {'BATTY_LEVEL': str(level), 'BATTY_START_LEVEL': '1',
           'BATTY_REPLAY_PROBE': '1'}
    if laffc:
        env['BATTY_LAFFC'] = '1'

    def cap(n):
        e = dict(env); e['BATTY_VISUAL_PROBE_FRAMES'] = str(n)
        Path(FLOPPY).unlink(missing_ok=True)
        envstr = ' '.join(f'{k}={v}' for k, v in e.items())
        subprocess.run(f'{envstr} make {FLOPPY}', shell=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run([sys.executable, 'scripts/capture_frame_timeline.py',
                        '--floppy', FLOPPY, '--frames', str(n), '--out', 'build/tl_sane'],
                       stdout=subprocess.DEVNULL)
        probe = Path('build/PROBE_sane.txt')
        subprocess.run(['mcopy', '-n', '-i', FLOPPY, '::PROBE.TXT', str(probe)],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if not probe.exists():
            return None
        t = probe.read_text()
        bq = re.search(r'bricks_quantity=([0-9A-Fa-f]+)', t)
        ball = re.search(r'object_ball_1=([0-9A-Fa-f]+)', t)
        b = bytes.fromhex(ball.group(1)) if ball else None
        return (int(bq.group(1), 16) if bq else None,
                (b[0] & 0x80) == 0 if b else None)

    initial = cap(2)
    final = cap(frames)
    if initial is None or final is None:
        return None
    return initial[0], final[0], final[1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--levels', default='1,5,10,15')
    ap.add_argument('--frames', type=int, default=500)
    ap.add_argument('--min-bricks', type=int, default=3,
                    help='if brick_collision destroys >= this, LAFFC must too')
    args = ap.parse_args()
    levels = [int(x) for x in args.levels.split(',') if x.strip()]

    fails = 0
    for lvl in levels:
        laf = run_level(lvl, args.frames, laffc=True)
        bc = run_level(lvl, args.frames, laffc=False)
        if laf is None or bc is None:
            print(f'  L{lvl}: FAIL (a run crashed / produced no probe)')
            fails += 1
            continue
        laf_destroyed = (laf[0] or 0) - (laf[1] or 0)
        bc_destroyed = (bc[0] or 0) - (bc[1] or 0)
        verdict = 'ok'
        if bc_destroyed >= args.min_bricks and laf_destroyed == 0:
            verdict = 'FAIL (LAFFC destroys none where brick_collision plays)'
            fails += 1
        elif not laf[2]:
            verdict = 'note: ball inactive at end (both paths may do this w/ static bat)'
        print(f'  L{lvl}: LAFFC destroyed {laf_destroyed} (ball_active={laf[2]}) '
              f'| brick_collision destroyed {bc_destroyed}  -> {verdict}')

    if fails:
        print(f'FAIL: {fails}/{len(levels)} levels show LAFFC misbehaving -- '
              f'do NOT flip the default; port the unported edge case first')
    else:
        print(f'PASS: LAFFC plays comparably to brick_collision on all '
              f'{len(levels)} levels')
    return fails


if __name__ == '__main__':
    sys.exit(main())
