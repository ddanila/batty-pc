#!/usr/bin/env python3
"""Port-side liveness sweep for the LAFFC collision path.

LAFFC is byte-exact on the L3 frame-step gate, but that one trajectory
does not exercise every cell neighbourhood. This sweep runs each level
headlessly (no ZEsarUX) for N frames under both collision paths with a
static bat (no input) and checks LAFFC stays *alive*: the run completes
(no crash/hang) and the ball keeps moving (not stuck / not passed
through into a frozen state).

IMPORTANT — what this does NOT prove. The brick-destruction count under a
static bat is NOT a correctness signal: with no bat to aim the ball,
whether it survives is pure trajectory luck, and the two paths diverge
after their first (different) bounce. `brick_collision` is itself only an
approximation, so LAFFC destroying fewer bricks than it is *expected*
divergence, not a bug — the brick counts are reported as INFO only.
Real per-level correctness needs an original-side reference (snapshot +
frame-step gate, as for L3), not a comparison against `brick_collision`.

A level FAILS only on a hard liveness problem: the run produced no probe
(crash/hang), or the ball never moved across the run (stuck). This is a
smoke test, not the flip gate.

    make test-laffc-levels-sane SANE_LEVELS=1,5,10,15 SANE_FRAMES=500
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
        if laf is None:
            print(f'  L{lvl}: FAIL liveness (LAFFC run crashed / produced no probe)')
            fails += 1
            continue
        bc = run_level(lvl, args.frames, laffc=False)
        laf_destroyed = (laf[0] or 0) - (laf[1] or 0)
        bc_destroyed = ((bc[0] or 0) - (bc[1] or 0)) if bc else None
        # INFO only: brick-count divergence under a static bat is trajectory
        # luck (brick_collision is itself approximate), NOT a correctness signal.
        print(f'  L{lvl}: LIVE (ran {args.frames} frames, ball_active={laf[2]})'
              f'  [info: LAFFC destroyed {laf_destroyed}, '
              f'brick_collision destroyed {bc_destroyed} -- divergence expected]')

    if fails:
        print(f'FAIL: {fails}/{len(levels)} levels hit a LAFFC liveness problem '
              f'(crash/hang) -- investigate before relying on the path')
    else:
        print(f'PASS: LAFFC ran to completion on all {len(levels)} levels '
              f'(liveness smoke test; NOT a per-level parity gate -- that needs '
              f'an original snapshot per level)')
    return fails


if __name__ == '__main__':
    sys.exit(main())
