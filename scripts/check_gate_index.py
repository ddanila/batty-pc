#!/usr/bin/env python3
"""Every gate the runner defines must be named in notes/testing.md.

Before the index existed, 30 of 59 gates were mentioned NOWHERE in that
file — including several of the oldest, like `test-bat-deflection` and
`test-enemy-descend`. The file had grown as a narrative of how particular
gates came to be, which is worth keeping, but it meant the question
"what covers this behaviour?" had no answer short of reading
`run_gates_parallel.py`.

This gate keeps the index complete. It deliberately does NOT check the
descriptions — nothing can — so a one-word entry would satisfy it. What
it buys is that adding a gate without saying what it is for becomes a
failing build rather than a silent omission, which is the failure that
actually happened 30 times.

The reverse direction is checked too: a name in the index that the runner
no longer defines is a gate someone deleted or renamed, and an index
listing gates that do not exist is worse than no index.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOC = ROOT / "notes" / "testing.md"
RUNNER = ROOT / "scripts" / "run_gates_parallel.py"
SECTION = "## Every gate, and what it is for"


MAKEFILE = ROOT / "Makefile"


def runner_gates() -> set[str]:
    """Every gate, from all THREE places they are defined.

    The first version of this gate read only run_gates_parallel.py and
    reported ten false "stale" entries, because the emulator-free source
    gates live in the Makefile's test-source-gates recipe and the
    ZEsarUX-oracle ones in parity-check-full. Three suites, three
    definitions — which is exactly why an index is worth having."""
    src = RUNNER.read_text()
    out: set[str] = set()
    for name in ("PARITY_CHECK_GATES", "FULL_EXTRA_GATES"):
        start = src.index(name + " = [")
        out |= set(re.findall(r'"([a-z0-9-]+)"', src[start:src.index("]", start)]))

    mk = MAKEFILE.read_text()
    for target in ("test-source-gates", "parity-check-full"):
        m = re.search(r"^" + re.escape(target) + r":.*?\n((?:\t.*\n|\s*\n)*)",
                      mk, re.M)
        if m:
            out |= set(re.findall(r"\$\(MAKE\) (test-[a-z0-9-]+|test)\b",
                                  m.group(1)))
    return out


def indexed_gates(doc: str) -> set[str]:
    if SECTION not in doc:
        raise SystemExit(
            f"FAIL: notes/testing.md has no '{SECTION}' section. That is the "
            f"index this gate keeps complete — restore it, or point this gate "
            f"at whatever replaced it.")
    body = doc.split(SECTION, 1)[1]
    nxt = body.find("\n## ")
    if nxt >= 0:
        body = body[:nxt]
    # Only LIST ITEMS count. Scanning the whole section swept up target
    # names from its own prose — the intro mentions `test-source-gates`
    # as a place gates are defined, and that read as an index entry.
    entries: set[str] = set()
    for line in body.split("\n"):
        if line.lstrip().startswith("- "):
            entries |= set(re.findall(r"`(test[a-z0-9-]*)`", line))
    return entries


def main() -> int:
    doc = DOC.read_text()
    gates = runner_gates()
    listed = indexed_gates(doc)

    missing = sorted(gates - listed)
    stale = sorted(listed - gates)

    if missing or stale:
        print("FAIL: notes/testing.md's gate index does not match the "
              "runner\n")
        for g in missing:
            print(f"  MISSING  {g} — the runner defines it, the index does "
                  f"not name it")
        for g in stale:
            print(f"  STALE    {g} — the index names it, the runner does not "
                  f"define it (deleted? renamed?)")
        print(f"\n{len(gates)} gates defined, {len(listed)} indexed. Add a "
              f"one-line entry under the right heading in "
              f"'{SECTION}'.")
        return 1

    print(f"PASS gate_index: all {len(gates)} gates are named in "
          f"notes/testing.md")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
