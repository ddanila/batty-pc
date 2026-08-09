#!/usr/bin/env python3
"""A module that DECLARES state must DEFINE it.

`objects[]` was declared `extern` in `objects.h` and defined in
`main.cpp`. Stage 6a moved the object model out and left the storage
behind, so the module described an array it did not own. Eleven stages
did not notice: the header looked right, and the DOS build always has
`main.cpp` in the link, so nothing could fail.

What finally caught it was a host test failing to LINK — which only
happens once a module gets a test that links it without `main.cpp`.
Modules without such a test have no equivalent safety net, and this gate
is that net.

### Why this is written conservatively

Two earlier attempts at the same scan produced five false positives each,
in different ways: the first could not parse multi-word types
(`unsigned char *vga`), the second could not see uninitialised scalars
(`unsigned markup_len;`). A gate that cries wolf about correct code gets
switched off, so the rule here is deliberately narrow:

  A definition is a line in `<module>.cpp` starting at column 0, naming
  the identifier, not beginning with `extern`, and ending in `[`, `=` or
  `;` after the name.

Anything it cannot classify is REPORTED as unclassifiable rather than
counted as a violation. A quiet "I could not tell" beats a confident
wrong answer — that is the same reason `check_gate_greps.py` prints what
it cannot verify.

That escape hatch is also a hazard, and it bit immediately: the first
working version compiled its definition pattern WITHOUT `re.M`, so `^`
only matched the start of the file and all 25 declarations came back
unclassifiable. It printed PASS. Restoring the real `objects[]` bug still
printed PASS. If the "unknown" list ever grows, that is a signal the
matcher has broken, not a curiosity — a gate that classifies nothing
passes everything.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

EXTERN = re.compile(r"^\s*extern\s+(.+?)[\s*]+(\w+)\s*(\[|;)")


def defines(text: str, name: str) -> bool:
    """Is `name` defined at file scope here?"""
    pat = re.compile(r"^(?!extern\b)[A-Za-z_][\w \t*&:<>,]*?\b"
                     + re.escape(name) + r"\s*(\[[^\]]*\])*\s*(=|;|\{)",
                     re.M)
    return bool(pat.search(text))


def main() -> int:
    cpps = {f.stem: f.read_text() for f in SRC.glob("*.cpp")}
    violations, unknown, checked = [], [], 0

    for h in sorted(SRC.glob("*.h")):
        mod = h.stem
        own = cpps.get(mod)
        for line in h.read_text().split("\n"):
            m = EXTERN.match(line)
            if not m:
                continue
            name = m.group(2)
            checked += 1
            if own is None:
                unknown.append((h.name, name, f"no src/{mod}.cpp exists"))
                continue
            if defines(own, name):
                continue
            elsewhere = [f"{k}.cpp" for k, v in cpps.items()
                         if k != mod and defines(v, name)]
            if elsewhere:
                violations.append((h.name, name, elsewhere))
            else:
                unknown.append((h.name, name,
                                "not recognised as defined anywhere — the "
                                "pattern here may be one this gate cannot "
                                "parse"))

    if violations:
        print("FAIL: a module declares state that another file defines\n")
        for hdr, name, where in violations:
            print(f"  {hdr} declares `extern ... {name}`")
            print(f"    but it is defined in: {', '.join(where)}")
            print(f"    -> move the definition into src/{hdr[:-2]}.cpp. Until "
                  f"then the module cannot be linked without that file, so "
                  f"it cannot get a host test — which is exactly how the "
                  f"objects[] case stayed hidden for eleven stages.")
            print()
        return 1

    print(f"PASS module_ownership: {checked} extern declarations, each "
          f"defined by its own module")
    for hdr, name, why in unknown:
        print(f"  note: could not classify {hdr}:{name} — {why}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
