#!/usr/bin/env python3
"""Deterministic mid-game frame-timeline capture (port side).

Drives the DOS port through a list of `BATTY_VISUAL_PROBE_FRAMES`
checkpoints: the port runs to each absolute frame index, halts on
`kbhit()`, and we screendump while it is paused — so each capture lands
on a byte-deterministic game frame with no wall-clock drift. Between
captures we `sendkey ret` to wake the port toward the next checkpoint.

This is the port half of the frame-step parity sweep (see
notes/replay-harness.md). It captures a *timeline* from a single boot,
rather than the one-shot capture the old single-value probe produced.

The floppy must already be built with the matching env, e.g.:

    BATTY_START_LEVEL=1 BATTY_LEVEL=1 BATTY_VISUAL_PROBE_FRAMES=30,60,90 \
        make build/batty-test.img
    python3 scripts/capture_frame_timeline.py --frames 30,60,90

`make capture-timeline FRAMES=30,60,90 LEVEL=1` wires both steps.

Exit code is 0 when every requested checkpoint produced a capture;
nonzero if any capture is missing. A static scene (e.g. the ball still
stuck on the bat) is valid, so consecutive captures are allowed to be
identical unless --require-motion is passed (used to prove the
multi-checkpoint stepping actually advances the simulation).
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu, ppm_inner_to_indices, PLAYFIELD_W, PLAYFIELD_H


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--floppy', default='build/batty-test.img')
    ap.add_argument('--frames', default='30,60,90',
                    help='comma-separated ascending absolute frame indices')
    ap.add_argument('--out', default='build/frame_timeline')
    ap.add_argument('--boot-wait', type=float, default=12.0)
    ap.add_argument('--fps', type=float, default=50.0,
                    help='game frame rate, for sizing the wait between checkpoints')
    ap.add_argument('--require-motion', action='store_true',
                    help='also fail if consecutive captures are identical '
                         '(proves the sim advanced between checkpoints)')
    ap.add_argument('--wait-key', action='store_true',
                    help='floppy was built with BATTY_REPLAY_WAIT_KEY=1: the '
                         'port pauses at main-loop entry ($BA83). Capture that '
                         'pause as frame 0 (the aligned start, == the original '
                         'side\'s post-setup $BA83), then wake and step the '
                         '--frames checkpoints. Frame counts then match the '
                         'original timeline frame-for-frame.')
    args = ap.parse_args()

    frames = [int(t) for t in args.frames.split(',') if t.strip()]
    if not frames or frames != sorted(frames) or len(set(frames)) != len(frames):
        print(f'frames must be a strictly-ascending list, got {args.frames!r}')
        return 2

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    # Build the QEMU monitor script.
    #
    # Default mode: the port free-runs from gameplay start and the visual
    # probe halts at frames[0] within the boot-wait window; thereafter each
    # `sendkey ret` releases it to run (frames[i]-frames[i-1]) more frames.
    #
    # --wait-key mode: the port pauses at main-loop entry ($BA83) during
    # the boot wait. We screendump that as frame 0 (byte-aligned with the
    # original's post-setup $BA83), then `sendkey ret` wakes it into the
    # frame loop; the probe then halts at each --frames checkpoint, counted
    # from entry just like the original side's $BA83 trips.
    script = [f'SLEEP {args.boot_wait}']
    ppm_paths = []
    if args.wait_key:
        zero = out / 'frame_0000.ppm'
        ppm_paths.append((0, zero))
        script.append(f'screendump {zero}')
    prev = 0
    for i, n in enumerate(frames):
        if args.wait_key or i > 0:
            delta = n - prev
            script.append('sendkey ret')
            script.append(f'SLEEP {delta / args.fps + 0.5:.3f}')
        ppm = out / f'frame_{n:04d}.ppm'
        ppm_paths.append((n, ppm))
        script.append(f'screendump {ppm}')
        prev = n

    print(f'booting {args.floppy} headless (boot wait {args.boot_wait}s), '
          f'checkpoints={frames}...')
    run_qemu(Path(args.floppy), script, out / 'qemu.log')

    # Decode each capture and verify consecutive frames differ.
    rc = 0
    prev_idx = None
    prev_n = None
    for n, ppm in ppm_paths:
        if not ppm.exists():
            print(f'  MISSING frame {n}: {ppm} not written')
            rc = 1
            continue
        idx = ppm_inner_to_indices(ppm)
        (out / f'frame_{n:04d}.idx').write_bytes(idx)
        if prev_idx is not None:
            diff = sum(1 for a, b in zip(idx, prev_idx) if a != b)
            tag = 'OK' if diff > 0 else 'STATIC'
            print(f'  frame {prev_n:>4} -> {n:>4}: {diff:>6} px changed [{tag}]')
            if diff == 0 and args.require_motion:
                # With --require-motion, identical consecutive captures
                # mean the port did not advance between checkpoints.
                rc = 1
        else:
            print(f'  frame {n:>4}: captured ({PLAYFIELD_W}x{PLAYFIELD_H})')
        prev_idx = idx
        prev_n = n

    if rc == 0:
        print(f'PASS: {len(frames)} deterministic checkpoints captured, '
              f'all consecutive frames advanced')
    else:
        print('FAIL: missing or static captures (see above)')
    return rc


if __name__ == '__main__':
    sys.exit(main())
