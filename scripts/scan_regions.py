#!/usr/bin/env python3
"""First-pass classifier for the Batty main blob.

Looks for three high-confidence data signatures:
  - ASCII text runs (>=4 printable chars, optionally terminated by a
    high-bit byte — ZX games often flip bit 7 on the last char).
  - Pointer tables: >=4 consecutive (lo, hi) pairs where hi is in the
    code range [0x68, 0xC2] (block 3 lives at 0x6800..0xC274).
  - Sparse-data regions: 32+ byte runs that are mostly 0x00 — typical
    of pre-init buffers and sprite working areas.

Anything not claimed by a data heuristic stays code by default.

Emits two files:
  build/regions.txt       human-readable report
  build/regions.blockdef  z80dasm --block-def format

Refine by hand once you've eyeballed the disassembly.
"""
import sys
from pathlib import Path

ORIGIN  = 0x6800
END     = 0xC274   # ORIGIN + 0x5A74
CODE_LO = 0x68
CODE_HI = 0xC2

MIN_TEXT_RUN     = 4
MIN_PTR_PAIRS    = 4
MIN_SPARSE       = 32
SPARSE_ZERO_FRAC = 0.60


def find_text(data):
    runs, i, n = [], 0, len(data)
    while i < n:
        if 0x20 <= data[i] <= 0x7E:
            j = i
            while j < n and 0x20 <= data[j] <= 0x7E:
                j += 1
            term_high = j < n and (data[j] & 0x80) and 0xA0 <= data[j] <= 0xFE
            length = j - i + (1 if term_high else 0)
            if j - i >= MIN_TEXT_RUN:
                runs.append((i, length, data[i:i + (j - i)].decode("latin-1")))
            i = j + (1 if term_high else 0)
        else:
            i += 1
    return runs


def find_pointer_tables(data):
    """Run of consecutive (lo, hi) pairs with hi in [CODE_LO, CODE_HI]."""
    tables, n = [], len(data)
    i = 0
    while i + 2 <= n:
        j = i
        pairs = 0
        while j + 2 <= n and CODE_LO <= data[j + 1] <= CODE_HI:
            pairs += 1
            j += 2
        if pairs >= MIN_PTR_PAIRS:
            tables.append((i, pairs * 2, pairs))
            i = j
        else:
            i += 1
    return tables


def find_sparse(data):
    """Sliding window: a region where >=60% of bytes are 0x00, length >=32."""
    runs, n = [], len(data)
    i = 0
    while i < n:
        if data[i] != 0:
            i += 1
            continue
        # Extend greedily while the running zero-fraction stays high.
        j = i
        zeros = 0
        best_end = i
        while j < n:
            if data[j] == 0:
                zeros += 1
            elif zeros / max(1, j - i + 1) < SPARSE_ZERO_FRAC:
                break
            j += 1
            if zeros / max(1, j - i) >= SPARSE_ZERO_FRAC:
                best_end = j
        if best_end - i >= MIN_SPARSE:
            runs.append((i, best_end - i))
            i = best_end
        else:
            i += 1
    return runs


def merge_intervals(spans):
    if not spans:
        return []
    spans = sorted(spans)
    merged = [list(spans[0])]
    for s, e in spans[1:]:
        if s <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], e)
        else:
            merged.append([s, e])
    return [tuple(m) for m in merged]


def main():
    if len(sys.argv) != 4:
        print("usage: scan_regions.py <blob> <report> <blockdef>", file=sys.stderr)
        sys.exit(2)
    data = Path(sys.argv[1]).read_bytes()
    if len(data) != END - ORIGIN:
        print(f"warn: expected {END - ORIGIN} bytes, got {len(data)}", file=sys.stderr)

    texts   = find_text(data)
    ptrs    = find_pointer_tables(data)
    sparses = find_sparse(data)

    text_spans   = [(o, o + l) for (o, l, _) in texts]
    ptr_spans    = [(o, o + l) for (o, l, _) in ptrs]
    sparse_spans = [(o, o + l) for (o, l)    in sparses]

    # Drop pointer-table candidates fully covered by ASCII text — those
    # are almost always false positives (text bytes happen to have hi
    # in code range).
    ptr_spans = [(s, e) for (s, e) in ptr_spans
                 if not any(ts <= s and e <= te for (ts, te) in text_spans)]

    # Report
    report = []
    def line(s): report.append(s)
    line(f"# Batty main blob region scan ({ORIGIN:#06x}..{END:#06x}, {len(data)} bytes)\n")
    line(f"## ASCII text runs ({len(texts)})")
    for off, length, txt in texts:
        addr = ORIGIN + off
        snippet = txt[:60].replace("\n", " ")
        line(f"  {addr:#06x}  len={length:3d}  {snippet!r}")
    line(f"\n## Pointer tables ({len(ptr_spans)})")
    for (s, e) in ptr_spans:
        pairs = (e - s) // 2
        line(f"  {ORIGIN+s:#06x}..{ORIGIN+e-1:#06x}  pairs={pairs}")
    line(f"\n## Sparse (zero-dominant) regions ({len(sparses)})")
    for off, length in sparses:
        line(f"  {ORIGIN+off:#06x}..{ORIGIN+off+length-1:#06x}  len={length}")

    Path(sys.argv[2]).write_text("\n".join(report) + "\n")
    print(f"wrote {sys.argv[2]}")

    # Block-def file for z80dasm. Resolve overlaps by length-priority
    # (longer span wins) — z80dasm refuses overlapping definitions.
    blocks = []
    for s, e in text_spans:   blocks.append((s, e, "bytedata", "text"))
    for s, e in ptr_spans:    blocks.append((s, e, "worddata", "ptrs"))
    for s, e in sparse_spans: blocks.append((s, e, "bytedata", "zeros"))
    blocks.sort(key=lambda b: (b[0], -(b[1] - b[0])))
    accepted, prev_end = [], 0
    for (s, e, btype, tag) in blocks:
        if s < prev_end:           # overlaps the previous accepted span
            continue
        accepted.append((s, e, btype, tag))
        prev_end = e
    out = ["; auto-generated by scan_regions.py — refine by hand."]
    for i, (s, e, btype, tag) in enumerate(accepted):
        addr_s = ORIGIN + s
        addr_e = ORIGIN + e
        out.append(f"{tag}_{i:03d}: start 0x{addr_s:04x} end 0x{addr_e:04x} type {btype}")
    Path(sys.argv[3]).write_text("\n".join(out) + "\n")
    print(f"wrote {sys.argv[3]}  ({len(accepted)} blocks, {len(blocks) - len(accepted)} overlaps dropped)")


if __name__ == "__main__":
    main()
