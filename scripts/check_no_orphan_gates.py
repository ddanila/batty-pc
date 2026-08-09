#!/usr/bin/env python3
"""Every gate-named script must be run by a Makefile target.

`scripts/test_*.py` and `scripts/check_*.py` are gates by convention.
One that no target invokes is worse than a missing gate: it looks like
coverage in a directory listing, it keeps passing when someone runs it
by hand, and nothing tells you the suite has stopped calling it.

Nothing is orphaned today — this is a fence, not a repair.

### Why the neighbouring check is NOT here

"Can this script fail?" was the obvious companion and is not decidable
by pattern. Six scripts looked failure-free to a first regex and were
fine: they end `sys.exit(main())` with `return fails`, a COUNT that is
non-zero when something failed. Two others use `raise SystemExit(...)`,
and the source gates mostly `return 1`.

That is three idioms for the same thing, and a checker that enumerates
them will call a working gate broken the day someone writes a fourth —
the same failure this repo has now hit four times with narrow patterns
(mcopy whitespace, known-bugs heading styles, asset_load variants,
`return 1`). So the six were checked by reading, recorded here, and left
ungated.

What IS gated instead: `test-gate-index` requires every Makefile gate
target to be named in notes/testing.md, and this requires every gate
script to be reachable from the Makefile. Between them a script cannot
go quiet without the suite noticing.
"""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAKEFILE = ROOT / "Makefile"


def main() -> int:
    mk = MAKEFILE.read_text()
    scripts = sorted((ROOT / "scripts").glob("test_*.py")) + \
              sorted((ROOT / "scripts").glob("check_*.py"))
    if not scripts:
        raise SystemExit("FAIL: found no gate-named scripts at all — the "
                         "naming convention changed and this checks nothing")

    orphans = [p.name for p in scripts if f"scripts/{p.name}" not in mk]
    if orphans:
        print(f"FAIL: {len(orphans)} gate script(s) no Makefile target "
              f"runs\n")
        for o in orphans:
            print(f"  scripts/{o}")
        print("\nAdd a target, or rename it out of the test_/check_ "
              "convention if it is a tool. scripts/visualise_levels.py is "
              "the worked example: it was called sweep_levels.py and read "
              "as a gate that could never fail.")
        return 1

    print(f"PASS no_orphan_gates: all {len(scripts)} gate scripts are run "
          f"by a Makefile target")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
