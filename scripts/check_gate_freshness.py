#!/usr/bin/env python3
"""A gate must not be satisfiable by a PREVIOUS run's output.

`test-visual-checkpoints` asserted that each checkpoint PPM `exists()`
and never cleared its output directory, so images left by an earlier run
satisfied it. It worked on a clean checkout and lied on a developer
machine that had run it once — the wrong way round, since the clean
checkout is the one place nobody runs it interactively.

Two shapes of stale output exist here, and both are checked:

  CAPTURES   a gate that reads or tests for files under its own
             `build/<name>/` directory must `rmtree` it first.

  PROBE.TXT  a gate that mcopies PROBE.TXT off the floppy must first
             guarantee a fresh one — either by rebuilding the floppy
             image from scratch (which carries no probe) or by `mdel`ing
             the probe off it.

The probe half currently finds nothing, and that is the point of writing
it down: 20 gates read PROBE.TXT without an `mdel`, which LOOKS wrong
until you notice they unlink and rebuild the image. Without this check
someone re-derives that analysis, or worse, "fixes" 20 safe gates.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "scripts"


def main() -> int:
    bad = []
    checked_captures = checked_probe = 0

    for f in sorted(SCRIPTS.glob("test_*.py")):
        t = f.read_text()

        # --- captures -------------------------------------------------
        m = re.search(r'^OUT\s*=\s*Path\("build/([\w/]+)"\)', t, re.M)
        if m:
            uses = re.search(r'OUT\s*/\s*[^)]*\)\s*\.exists\(\)', t) \
                or re.search(r'ppm_inner_to_indices\(\s*OUT\s*/', t) \
                or re.search(r'read_bytes\(\)|read_text\(\)', t)
            if uses:
                checked_captures += 1
                if "rmtree" not in t:
                    bad.append((f.name, "reads or tests for files under its "
                                        "own build/ directory but never "
                                        "rmtree's it — a previous run's "
                                        "output can satisfy it"))

        # --- PROBE.TXT ------------------------------------------------
        if re.search(r"mcopy[^\n]*PROBE\.TXT", t):
            checked_probe += 1
            # Both halves must be about the FLOPPY. Matching any
            # `unlink(missing_ok=True)` anywhere was too loose: the local
            # probe's own unlink satisfied it, so deleting a gate's
            # floppy rebuild still passed.
            rebuilds = re.search(r"[A-Za-z_]*FLOPPY\)?\.unlink\(", t) and \
                       re.search(r"make\s+\{?[A-Za-z_]*FLOPPY", t)
            mdels = re.search(r"mdel[^\n]*PROBE\.TXT", t)
            if not (rebuilds or mdels):
                bad.append((f.name, "mcopies PROBE.TXT without either "
                                    "rebuilding the floppy or mdel'ing the "
                                    "probe first, so a failed run can grade "
                                    "the previous run's probe"))

    if not checked_captures and not checked_probe:
        raise SystemExit("FAIL: matched no gates at all — the OUT/mcopy "
                         "patterns changed and this gate is checking nothing")

    if bad:
        print("FAIL: a gate can be satisfied by a previous run's output\n")
        for name, why in bad:
            print(f"  {name}")
            print(f"    {why}")
        print("\nAdd `shutil.rmtree(OUT, ignore_errors=True)` at the top of "
              "main(), or clear the probe before reading it.")
        return 1

    print(f"PASS gate_freshness: {checked_captures} capture-reading and "
          f"{checked_probe} probe-reading gates all start clean")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
