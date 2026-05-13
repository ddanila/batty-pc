#!/usr/bin/env python3
"""Render every bytedata candidate region as a 1bpp PNG strip.

For each region in regions.blockdef of type `bytedata` (and >= MIN_SIZE
bytes), render the bytes as 1bpp graphics at four byte-widths
(1/2/3/4 bytes wide = 8/16/24/32 px wide) and lay them out side-by-side
in one PNG. The user scrolls through assets/candidates/ and labels
each region as "sprite at width=N" or "not a sprite".

Also dump one whole-blob view at width=2 as a fallback so anything
the scanner missed is still visible somewhere.
"""
import re
import sys
from pathlib import Path

from PIL import Image

WIDTHS = [1, 2, 3, 4]    # bytes per row
ZOOM   = 3
GAP    = 8               # px between width-columns
MIN_SIZE = 16
ORIGIN = 0x6800


def render_strip(data: bytes, width_bytes: int) -> Image.Image:
    """1bpp top-down: byte 0 fills top row's leftmost 8 px, MSB first."""
    n = len(data)
    rows = (n + width_bytes - 1) // width_bytes
    w_px = width_bytes * 8
    img = Image.new("L", (w_px, rows), 0)
    px = img.load()
    for i, b in enumerate(data):
        row = i // width_bytes
        col0 = (i % width_bytes) * 8
        for bit in range(8):
            if (b >> (7 - bit)) & 1:
                px[col0 + bit, row] = 255
    return img


def composite(data: bytes) -> Image.Image:
    strips = [render_strip(data, w) for w in WIDTHS]
    # Pad shorter strips so they line up vertically.
    max_h = max(s.height for s in strips)
    total_w = sum(s.width for s in strips) + GAP * (len(strips) - 1)
    out = Image.new("L", (total_w, max_h), 32)   # dark grey gap
    x = 0
    for s in strips:
        out.paste(s, (x, 0))
        x += s.width + GAP
    return out.resize((out.width * ZOOM, out.height * ZOOM), Image.NEAREST)


BLOCK_RE = re.compile(
    r"^\s*\S+:\s+start\s+0x([0-9a-fA-F]+)\s+end\s+0x([0-9a-fA-F]+)\s+type\s+(\S+)"
)


def parse_blockdef(path: Path):
    out = []
    for ln in path.read_text().splitlines():
        m = BLOCK_RE.match(ln)
        if not m:
            continue
        start, end, btype = int(m[1], 16), int(m[2], 16), m[3]
        out.append((start, end, btype))
    return out


def main():
    if len(sys.argv) != 4:
        print("usage: render_candidates.py <blob> <blockdef> <out_dir>",
              file=sys.stderr)
        sys.exit(2)
    blob = Path(sys.argv[1]).read_bytes()
    blocks = parse_blockdef(Path(sys.argv[2]))
    out_dir = Path(sys.argv[3])
    out_dir.mkdir(parents=True, exist_ok=True)

    index_lines = [
        "# Batty sprite candidates",
        "# Layout per PNG: 4 columns side-by-side, widths 1/2/3/4 bytes",
        f"# Zoom: {ZOOM}x.  Origin: {ORIGIN:#06x}.",
        "",
        f"{'idx':>3}  {'addr':>6}  {'size':>5}  type",
    ]
    kept = 0
    for i, (start, end, btype) in enumerate(blocks):
        if btype != "bytedata":
            continue
        size = end - start
        if size < MIN_SIZE:
            continue
        off = start - ORIGIN
        data = blob[off:off + size]
        if not data:
            continue
        img = composite(data)
        name = f"{start:04x}_{size:04d}B.png"
        img.save(out_dir / name)
        index_lines.append(f"{kept:>3}  0x{start:04x}  {size:>5}  {btype}  -> {name}")
        kept += 1

    # Whole-blob views — one per width. These catch sprites the scanner
    # didn't flag (everything still classified as 'code' by default).
    for w in WIDTHS:
        strip = render_strip(blob, w)
        scaled = strip.resize((strip.width * 2, strip.height * 2), Image.NEAREST)
        scaled.save(out_dir / f"_whole_w{w}.png")
        index_lines.append(f"Whole-blob (width={w}, 2x zoom) -> _whole_w{w}.png")

    (out_dir / "index.txt").write_text("\n".join(index_lines) + "\n")
    print(f"rendered {kept} per-region PNGs + whole-blob view in {out_dir}/")


if __name__ == "__main__":
    main()
