#!/usr/bin/env python3
"""Ad-hoc: per-frame A/B diff of the fly-over-trail checkpoint timelines
(build/tl_trail_dirty vs build/tl_trail_full). Prints count + bounds +
pixel list per captured frame."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import ppm_inner_to_indices, PALETTE_RGB

W = 256
frames = [int(t) for t in (sys.argv[1] if len(sys.argv) > 1
                           else "12,20,30,40,44,48,50").split(",")]
for fr in frames:
    dp = Path(f"build/tl_trail_dirty/frame_{fr:04d}.ppm")
    fp = Path(f"build/tl_trail_full/frame_{fr:04d}.ppm")
    if not (dp.exists() and fp.exists()):
        print(f"frame {fr:3d}: MISSING capture")
        continue
    d = ppm_inner_to_indices(dp)
    f = ppm_inner_to_indices(fp)
    diffs = [(x, y, d[y * W + x], f[y * W + x])
             for y in range(192) for x in range(W)
             if PALETTE_RGB[d[y * W + x]] != PALETTE_RGB[f[y * W + x]]]
    if diffs:
        xs = [x for x, _, _, _ in diffs]
        ys = [y for _, y, _, _ in diffs]
        print(f"frame {fr:3d}: {len(diffs):4d} px  "
              f"bounds=({min(xs)},{min(ys)})..({max(xs)},{max(ys)})")
        for x, y, a, b in diffs[:30]:
            print(f"    ({x:3d},{y:3d}) dirty={a:2d} full={b:2d}")
        if len(diffs) > 30:
            print(f"    ... +{len(diffs)-30} more")
    else:
        print(f"frame {fr:3d}:    0 px")
