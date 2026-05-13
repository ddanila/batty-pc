#!/usr/bin/env python3
"""Search snapshot screen RAM for byte sequences from our sprite regions.

Two passes per region:
  1. Raw source bytes (matches if sprite is drawn at a byte-aligned X).
  2. Pre-shifted copies from sub_6853h's destination addresses (matches
     if sprite is drawn at a sub-byte-aligned X, using one of shifts 1..7).

Each candidate run is 8+ consecutive non-zero bytes — sprite-y enough
to be distinctive but not "00 00 00..." (which appears everywhere).
Reports snapshot, screen offset, length matched.

ZX screen RAM = 0x4000..0x5AFF (6912 bytes); this lives at offset 0
through 6912 in the snapshot's ram_4000_FFFF.bin.
"""
import sys
from pathlib import Path

ORIGIN = 0x6800
SRC_REGIONS = [
    # (label, src_addr, dst_addr_for_shifts, n_shifts, total_pre_shifted_bytes)
    # n_shifts: how many pre-shifted copies sub_6853h made (7 for entry 0, 1 for entries 1-3)
    ("region0_7B16", 0x7B16, 0x7B48, 7),
    ("region1_7E38", 0x7E38, 0x7D4E, 1),
    ("region2_7F42", 0x7F42, 0x828A, 1),
    ("region3_8188", 0x8188, 0x81F2, 1),
]
MIN_RUN = 8
VRAM_LEN = 0x1B00     # 6912


def find_runs(data: bytes, start: int, length: int, min_run: int):
    """Yield (offset, run_bytes) for each ≥min_run nonzero contiguous run."""
    out = []
    i, n = 0, length
    while i < n:
        if data[start + i] == 0:
            i += 1
            continue
        j = i
        while j < n and data[start + j] != 0:
            j += 1
        if j - i >= min_run:
            out.append((i, bytes(data[start + i : start + j])))
        i = j
    return out


def search_in(haystack: bytes, needle: bytes):
    """All offsets where needle occurs in haystack."""
    hits, off = [], 0
    while True:
        i = haystack.find(needle, off)
        if i < 0: break
        hits.append(i)
        off = i + 1
    return hits


def main():
    blob = Path('original/blocks/03_DATA_headless.dat.bin').read_bytes()
    snaps_dir = Path('build/snapshots')
    snap_files = sorted(snaps_dir.glob('*/ram_4000_FFFF.bin'))
    if not snap_files:
        print("no snapshots in build/snapshots/", file=sys.stderr)
        sys.exit(1)

    print(f"snapshots: {len(snap_files)}\n")
    for label, src_addr, dst_addr, n_shifts in SRC_REGIONS:
        o_src = src_addr - ORIGIN
        width_chunks, height = blob[o_src], blob[o_src + 1]
        body_size = width_chunks * height * 2
        # Pre-shifted region size: dst_dword_count * n_shifts.
        # Per outer iter sub_688Bh writes (2*byte0+2) bytes. byte1 outers.
        per_shift = 2 + height * (2 * width_chunks + 2)
        shifted_size = per_shift * n_shifts
        print(f"=== {label}  src=0x{src_addr:04X}  body={body_size}B"
              f"  dst=0x{dst_addr:04X}  pre-shifted={shifted_size}B"
              f"  ({width_chunks*16}x{height})")

        # Pass 1: search SOURCE bytes (the unshifted sprite).
        runs_src = find_runs(blob, o_src + 2, body_size, MIN_RUN)
        # Pass 2: search PRE-SHIFTED bytes.
        runs_pre = find_runs(blob, dst_addr - ORIGIN, shifted_size, MIN_RUN)

        for sf in snap_files:
            ts = sf.parent.name
            ram = sf.read_bytes()
            vram = ram[:VRAM_LEN]
            n_src = sum(len(search_in(vram, r)) for _, r in runs_src)
            n_pre = sum(len(search_in(vram, r)) for _, r in runs_pre)
            best_src = max((len(r) for _, r in runs_src
                            if search_in(vram, r)), default=0)
            best_pre = max((len(r) for _, r in runs_pre
                            if search_in(vram, r)), default=0)
            print(f"  {ts}  src_hits={n_src:3d} (best={best_src}B)   "
                  f"pre_hits={n_pre:3d} (best={best_pre}B)")
        print()


if __name__ == "__main__":
    main()
