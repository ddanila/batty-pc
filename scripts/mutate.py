#!/usr/bin/env python3
"""Apply a mutation, run a test target, report whether it was caught.

Mutation testing found five real gaps in this repo's own tests — a gate
that skipped a third of the gates, two suites that tested the shape of a
value and not the value, and zxvga never checking where a sprite lands.
It also went wrong three separate ways in the process, twice producing a
confident but false result. This exists so it goes wrong less.

The three failure modes, all of which this handles:

  STALE BINARY, same-second.  Restore a source file within the same
      second as the previous build and make sees an unchanged timestamp,
      reruns the OLD binary, and the mutation looks caught.

  STALE BINARY, wrong name.  `make test-video` builds `build/test_zxvga`.
      Deleting "build/test_video" deletes nothing, so every run used a
      stale binary and a real gap was reported as caught. Only a restored
      source STILL failing — which cannot happen — exposed it. This
      script deletes every build/test_* FILE rather than guessing which
      one a target builds. Directories are left alone: the QEMU gates
      keep their captures there.

  SILENT NO-OP.  A substitution that matches nothing leaves the source
      unmutated, the test passes, and it reads as "not caught" — the
      most dangerous outcome, because it looks like a finding. The file
      is compared before and after and a no-op is an error, not a result.

Usage:
    scripts/mutate.py <file> <find> <replace> <make-target> [label]

Exit status is 0 when the mutation was CAUGHT (the desirable outcome),
1 when it survived, 2 when the mutation could not be applied.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def clear_test_binaries() -> int:
    build = ROOT / "build"
    if not build.is_dir():
        return 0
    n = 0
    for p in build.glob("test_*"):
        if p.is_file():
            p.unlink()
            n += 1
    return n


def main() -> int:
    if len(sys.argv) < 5:
        print(__doc__)
        return 2
    path, find, replace, target = sys.argv[1:5]
    label = sys.argv[5] if len(sys.argv) > 5 else f"{Path(path).name}: {find}"

    f = ROOT / path
    original = f.read_text()
    if find not in original:
        print(f"ERROR [{label}]: the text to replace is not in {path}.")
        print("  Nothing was mutated, so a passing test would mean nothing.")
        return 2
    mutated = original.replace(find, replace, 1)
    if mutated == original:
        print(f"ERROR [{label}]: substitution changed nothing")
        return 2

    try:
        f.write_text(mutated)
        clear_test_binaries()
        r = subprocess.run(["make", target], cwd=ROOT,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        caught = r.returncode != 0
    finally:
        f.write_text(original)
        clear_test_binaries()

    print(f"{'caught  ' if caught else 'SURVIVED'}  {label}")
    if not caught:
        print(f"           `make {target}` passed with {path} mutated:")
        print(f"             {find.strip()}  ->  {replace.strip()}")
        print(f"           Either the tests do not cover it, or it is an "
              f"equivalent mutant. Decide which, and say so.")
    return 0 if caught else 1


if __name__ == "__main__":
    raise SystemExit(main())
