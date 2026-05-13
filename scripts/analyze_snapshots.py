#!/usr/bin/env python3
"""Classify every byte of 0x4000..0xFFFF using N snapshots + the on-tape blob.

Classes:
  TAPE     — in [0x6800..0xC274) AND all snapshots agree AND matches on-tape byte
  STATIC   — all snapshots agree but doesn't match tape (or outside tape range);
             includes precomputed tables, loader scratch, never-touched RAM
  DYNAMIC  — at least one snapshot differs from the others (live game state)

Outputs:
  build/ram_map.txt          run-length-compressed region map with hints
  build/ram_smc.txt          tape-range bytes that disagree with on-tape blob
                              (self-modifying code patches, if any)
"""
import sys
from pathlib import Path

RAM_START   = 0x4000
RAM_END     = 0x10000
TAPE_START  = 0x6800
TAPE_END    = 0xC274

REGION_HINTS = [
    (0x4000, 0x5800, "screen pixels"),
    (0x5800, 0x5B00, "screen attrs"),
    (0x5B00, 0x5C00, "printer buf"),
    (0x5C00, 0x5CB6, "ZX sysvars"),
    (0x5CB6, 0x6000, "BASIC free / game stack"),
    (0x6000, 0x6800, "below load address"),
    (0x6800, 0xC274, "tape blob area"),
    (0xC274, 0xF200, "post-blob (unpacked content?)"),
    (0xF200, 0xF400, "shift table (built by loader)"),
    (0xF400, 0x10000, "post-shift-table"),
]


def region_hint(addr):
    for s, e, name in REGION_HINTS:
        if s <= addr < e:
            return name
    return "?"


def main():
    if len(sys.argv) < 5:
        print("usage: analyze_snapshots.py <tape.bin> <out_dir> <snap1.bin> [snap2.bin...]",
              file=sys.stderr)
        sys.exit(2)
    tape = Path(sys.argv[1]).read_bytes()
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    snaps = [Path(p).read_bytes() for p in sys.argv[3:]]
    n = RAM_END - RAM_START
    for s, path in zip(snaps, sys.argv[3:]):
        if len(s) != n:
            raise SystemExit(f"{path}: expected {n} bytes, got {len(s)}")

    classes = bytearray(n)   # 0=TAPE, 1=STATIC, 2=DYNAMIC
    smc_hits = []
    for i in range(n):
        addr = RAM_START + i
        b0 = snaps[0][i]
        all_same = all(s[i] == b0 for s in snaps[1:])
        if not all_same:
            classes[i] = 2
            continue
        if TAPE_START <= addr < TAPE_END:
            t = tape[addr - TAPE_START]
            if b0 == t:
                classes[i] = 0
            else:
                classes[i] = 1
                smc_hits.append((addr, t, b0))
        else:
            classes[i] = 1

    labels = {0: "TAPE", 1: "STATIC", 2: "DYNAMIC"}
    lines = []
    i = 0
    while i < n:
        cls, j = classes[i], i
        while j < n and classes[j] == cls:
            j += 1
        start_addr = RAM_START + i
        end_addr   = RAM_START + j - 1
        length     = j - i
        rstart = region_hint(start_addr)
        rend   = region_hint(end_addr)
        rname  = rstart if rstart == rend else f"{rstart} -> {rend}"
        lines.append(f"0x{start_addr:04X}..0x{end_addr:04X}  ({length:>6} B)  "
                     f"{labels[cls]:>7}  {rname}")
        i = j

    (out_dir / "ram_map.txt").write_text("\n".join(lines) + "\n")

    smc_lines = []
    if smc_hits:
        # Compact: collapse contiguous runs.
        run_start, run_tape, run_ram = smc_hits[0]
        cur = run_start
        for addr, t, b in smc_hits[1:]:
            if addr == cur + 1:
                cur = addr
                continue
            smc_lines.append(f"0x{run_start:04X}..0x{cur:04X}  ({cur-run_start+1} B)")
            run_start = addr
            cur = addr
        smc_lines.append(f"0x{run_start:04X}..0x{cur:04X}  ({cur-run_start+1} B)")
    (out_dir / "ram_smc.txt").write_text(
        f"{len(smc_hits)} byte(s) in tape range disagree with on-tape blob\n"
        + ("(none — no self-modifying code seen)\n" if not smc_hits
           else "\n".join(smc_lines) + "\n")
    )

    total = [classes.count(c) for c in (0, 1, 2)]
    print(f"tape-match: {total[0]:>6} B  ({100*total[0]/n:5.1f}%)")
    print(f"static:     {total[1]:>6} B  ({100*total[1]/n:5.1f}%)")
    print(f"dynamic:    {total[2]:>6} B  ({100*total[2]/n:5.1f}%)")
    print(f"SMC hits:   {len(smc_hits)}")
    print(f"wrote {out_dir}/ram_map.txt, ram_smc.txt")


if __name__ == "__main__":
    main()
