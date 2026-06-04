#!/usr/bin/env python3
"""Diff a port-side frame timeline against an original-side one.

Both capture_frame_timeline.py (port) and capture_frame_timeline_original.py
(original) write per-checkpoint `frame_NNNN.idx` buffers in the same
256x192 palette-index space. This compares the two dirs frame-for-frame
and reports the per-frame pixel delta — the frame-step parity gate (see
notes/replay-harness.md).

The comparison is in palette-index space, identical to test_visual.py /
replay_harness.py. The first frame at which the delta jumps is where the
two simulations diverge, which is the actionable parity signal.

Usage:
    python3 scripts/compare_timelines.py \
        --port build/frame_timeline --original build/frame_timeline_original \
        --frames 1,3,5 [--max-diff 0]

Exit code 0 when every compared frame is within --max-diff pixels;
nonzero (the number of failing frames) otherwise.
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import PLAYFIELD_W, PLAYFIELD_H, PALETTE_RGB

NPX = PLAYFIELD_W * PLAYFIELD_H

# Compare in RGB palette space, matching test_visual.py / replay_harness.py:
# bright-black (index 8) and non-bright black (index 0) both render as
# (0,0,0), so they must count as equal. Map each palette index to its RGB
# and compare those, not the raw index (which over-counts visually
# identical pixels — e.g. the whole black background as 8-vs-0 noise).
_IDX_RGB = [PALETTE_RGB[i] for i in range(16)]


def load_idx(path: Path) -> bytes:
    data = path.read_bytes()
    if len(data) != NPX:
        raise ValueError(f"{path}: expected {NPX} bytes, got {len(data)}")
    return data


def diff_bounds(a: bytes, b: bytes, roi=None):
    """Return (count, (x0, y0, x1, y1)) of differing pixels within roi.

    roi is (x0, y0, x1, y1) with x1/y1 exclusive; None = whole playfield.
    """
    rx0, ry0, rx1, ry1 = roi if roi else (0, 0, PLAYFIELD_W, PLAYFIELD_H)
    count = 0
    bx0 = by0 = 1 << 30
    bx1 = by1 = -1
    for y in range(ry0, ry1):
        base = y * PLAYFIELD_W
        for x in range(rx0, rx1):
            i = base + x
            if a[i] != b[i] and _IDX_RGB[a[i]] != _IDX_RGB[b[i]]:
                count += 1
                bx0 = min(bx0, x); by0 = min(by0, y)
                bx1 = max(bx1, x); by1 = max(by1, y)
    bounds = None if bx1 < 0 else (bx0, by0, bx1, by1)
    return count, bounds


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', default='build/frame_timeline')
    ap.add_argument('--original', default='build/frame_timeline_original')
    ap.add_argument('--frames', required=True,
                    help='comma-separated frame indices present in both dirs')
    ap.add_argument('--max-diff', type=int, default=0,
                    help='per-frame pixel-diff ceiling for PASS (default 0)')
    ap.add_argument('--roi', default=None,
                    help='x0,y0,x1,y1 (x1/y1 exclusive) to restrict the diff; '
                         'e.g. 8,32,248,128 = the brick play region used by '
                         'the replay-l3-entry gate')
    args = ap.parse_args()

    frames = [int(t) for t in args.frames.split(',') if t.strip()]
    roi = tuple(int(v) for v in args.roi.split(',')) if args.roi else None
    if roi is not None and len(roi) != 4:
        print(f'--roi must be x0,y0,x1,y1, got {args.roi!r}')
        return 2
    npx = (roi[2] - roi[0]) * (roi[3] - roi[1]) if roi else NPX
    port = Path(args.port)
    orig = Path(args.original)

    failures = 0
    for n in frames:
        pp = port / f'frame_{n:04d}.idx'
        op = orig / f'frame_{n:04d}.idx'
        if not pp.exists() or not op.exists():
            miss = pp if not pp.exists() else op
            print(f'  frame {n:>4}: MISSING {miss}')
            failures += 1
            continue
        count, bounds = diff_bounds(load_idx(pp), load_idx(op), roi)
        tag = 'PASS' if count <= args.max_diff else 'FAIL'
        if count > args.max_diff:
            failures += 1
        loc = f' bounds={bounds}' if bounds else ''
        print(f'  frame {n:>4}: {count:>6}/{npx} px differ [{tag}]{loc}')

    roi_note = f' roi={roi}' if roi else ''
    if failures == 0:
        print(f'PASS: {len(frames)} frames within {args.max_diff}px '
              f'(port == original){roi_note}')
    else:
        print(f'FAIL: {failures}/{len(frames)} frames exceed {args.max_diff}px')
    return failures


if __name__ == '__main__':
    sys.exit(main())
