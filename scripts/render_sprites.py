#!/usr/bin/env python3
"""Render the 4 sprite regions identified via sub_6853h's table.

Correct format (per careful re-trace of sub_688Bh):
  byte 0 = chunks-per-row (width in 16-px units)  → inner loop count
  byte 1 = rows-per-sprite (height)               → outer loop count
  body   = byte1 rows × byte0 chunks/row × 2 bytes/chunk

So this region is ONE sprite of width = byte0*16, height = byte1.
"""
import sys
from pathlib import Path

from PIL import Image

ORIGIN = 0x6800
REGIONS = [0x7B16, 0x7E38, 0x7F42, 0x8188]
ZOOM = 4


def render_sprite(body: bytes, width_chunks: int, height: int) -> Image.Image:
    w_px = width_chunks * 16
    img = Image.new("L", (w_px, height), 0)
    px = img.load()
    for r in range(height):
        row_off = r * width_chunks * 2
        for c in range(width_chunks):
            hi = body[row_off + c * 2]
            lo = body[row_off + c * 2 + 1]
            x_base = c * 16
            for b in range(8):
                if (hi >> (7 - b)) & 1: px[x_base + b,     r] = 255
                if (lo >> (7 - b)) & 1: px[x_base + 8 + b, r] = 255
    return img


def main():
    blob = Path('original/blocks/03_DATA_headless.dat.bin').read_bytes()
    out_dir = Path('assets/sprites_v2')
    out_dir.mkdir(parents=True, exist_ok=True)

    index = ["# sprite renders v2 — width=byte0*16, height=byte1"]
    for addr in REGIONS:
        o = addr - ORIGIN
        width_chunks = blob[o]
        height       = blob[o + 1]
        body_size    = width_chunks * height * 2
        body         = blob[o + 2 : o + 2 + body_size]
        index.append(f"\n## 0x{addr:04X}  width={width_chunks*16}px  height={height}  body={body_size}B")
        img = render_sprite(body, width_chunks, height)
        scaled = img.resize((img.width * ZOOM, img.height * ZOOM), Image.NEAREST)
        scaled.save(out_dir / f"{addr:04X}.png")
        index.append(f"  -> {addr:04X}.png")

    (out_dir / "index.txt").write_text("\n".join(index) + "\n")
    print(f"wrote {sum(1 for _ in out_dir.iterdir())} files in {out_dir}/")


if __name__ == "__main__":
    main()
