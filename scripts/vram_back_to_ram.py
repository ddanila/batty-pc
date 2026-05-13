#!/usr/bin/env python3
"""Reverse-search: for each non-zero run in a snapshot's VRAM, find every
match in non-VRAM RAM. Tells us where the sprite/font bytes painted on
screen actually live.

VRAM is the first 6912 bytes of the snapshot (0x4000..0x5AFF in absolute).
Non-VRAM RAM is everything from 0x5B00..0xFFFF (snap offset 0x1B00 onward).
"""
import sys
from pathlib import Path
from collections import defaultdict

VRAM_LEN = 0x1B00       # 6912
RAM_BASE = 0x4000
MIN_RUN  = 8


def find_runs(data: bytes, length: int, min_run: int):
    out, i = [], 0
    while i < length:
        if data[i] == 0:
            i += 1; continue
        j = i
        while j < length and data[j] != 0:
            j += 1
        if j - i >= min_run:
            out.append((i, bytes(data[i:j])))
        i = j
    return out


def find_all(haystack: bytes, needle: bytes):
    hits, off = [], 0
    while True:
        i = haystack.find(needle, off)
        if i < 0: break
        hits.append(i)
        off = i + 1
    return hits


def main():
    snap = Path(sys.argv[1])
    ram = snap.read_bytes()                  # 49152 bytes, 0x4000..0xFFFF
    vram = ram[:VRAM_LEN]
    non_vram = ram                            # search everywhere; we'll
                                              # filter out self-matches.

    pixel_vram = vram[:0x1800]   # 0x4000..0x57FF — pixel area; skip attrs.
    runs = find_runs(pixel_vram, len(pixel_vram), MIN_RUN)
    print(f"# {snap.parent.name}")
    print(f"# {len(runs)} distinctive VRAM runs (>={MIN_RUN}B non-zero)")
    print()

    by_source = defaultdict(list)       # source_start -> [(vram_off, length)]
    multi_source = 0
    for v_off, needle in runs:
        hits = find_all(non_vram, needle)
        # Drop the trivial self-match in VRAM.
        hits = [h for h in hits if h != v_off]
        # Drop matches inside VRAM (any other in-screen replicate).
        hits = [h for h in hits if h >= VRAM_LEN]
        if not hits:
            continue
        for h in hits:
            by_source[h].append((v_off, len(needle)))
        if len(hits) > 1:
            multi_source += 1

    # Cluster sources by 256-byte page to make output digestible.
    pages = defaultdict(int)
    for src, vmatches in by_source.items():
        pages[(RAM_BASE + src) >> 8] += sum(L for _, L in vmatches)
    print(f"Source addresses with VRAM matches (collapsed to 256-byte pages):\n")
    print(f"{'page':>8}  {'addr':>11}  {'bytes':>7}")
    for page, bytes_total in sorted(pages.items(), key=lambda kv: -kv[1])[:40]:
        addr_lo = page << 8
        addr_hi = addr_lo + 0xFF
        print(f"  0x{page:04X}  0x{addr_lo:04X}-{addr_hi:04X}  {bytes_total:>6}B")

    # Individual top sources.
    print(f"\nTop individual source offsets (largest blocks of source bytes):\n")
    src_sizes = [(src, sum(L for _, L in v)) for src, v in by_source.items()]
    src_sizes.sort(key=lambda x: -x[1])
    for src, total in src_sizes[:20]:
        print(f"  RAM 0x{RAM_BASE+src:04X}  total {total}B used in VRAM")


if __name__ == "__main__":
    main()
