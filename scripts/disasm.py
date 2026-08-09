#!/usr/bin/env python3
"""Print a routine from the original's disassembly.

Three hypotheses of mine died on contact with `original/disasm/batty.asm`
in one week — $38 as a transcription slip, the bat resize growing twice
too fast, and an invented $10 rotation in the enemy's motion. Each guess
was plausible, each trace was two minutes, and I guessed first every
time. Part of that is friction: reading a routine meant
`sed -n "$(grep -n '^name:' ...)"` and counting lines by eye.

    scripts/disasm.py handling_bird       # by label
    scripts/disasm.py 0xA67B              # by address, via the
                                          # "; Routine at XXXX" headers
    scripts/disasm.py check_ -l           # list labels matching a substring

A routine runs from its label to the next top-level label, so calls into
the middle of one (LA67B_8 and friends) are named labels too and print
just their own stretch — which is usually what you want.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASM = ROOT / "original" / "disasm" / "batty.asm"
LABEL = re.compile(r"^([A-Za-z_][A-Za-z_0-9]*):")


def load():
    lines = ASM.read_text(errors="replace").split("\n")
    labels = {}
    for i, l in enumerate(lines):
        m = LABEL.match(l)
        if m:
            labels.setdefault(m.group(1), i)
    return lines, labels


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    listing = "-l" in sys.argv
    if not args:
        print(__doc__)
        return 2
    want = args[0]
    lines, labels = load()

    if listing:
        hits = sorted(n for n in labels if want.lower() in n.lower())
        if not hits:
            print(f"no label contains {want!r}")
            return 1
        for n in hits:
            print(f"  {n:28s} line {labels[n] + 1}")
        return 0

    start = labels.get(want)
    if start is None:
        # An address: find its "; Routine at XXXX" header, then the label.
        addr = want.upper().removeprefix("0X").removeprefix("$")
        for i, l in enumerate(lines):
            if re.match(rf"^;\s*Routine at {addr}\b", l, re.I):
                for j in range(i, min(i + 12, len(lines))):
                    if LABEL.match(lines[j]):
                        start = j
                        break
                break
    if start is None:
        near = sorted(n for n in labels if want.lower() in n.lower())[:6]
        print(f"no label or routine header for {want!r}"
              + (f"; did you mean: {', '.join(near)}" if near else "")
              + "\nTry `scripts/disasm.py <substring> -l` to list labels.")
        return 1

    end = len(lines)
    for j in range(start + 1, len(lines)):
        if LABEL.match(lines[j]):
            end = j
            break
    # The NEXT routine's comment header sits above its label, so trim
    # trailing comment/blank lines — they describe what comes after.
    body = lines[start:end]
    while body and (not body[-1].strip() or body[-1].lstrip().startswith(";")):
        body.pop()
    print("\n".join(body))

    # FALLTHROUGH. A routine that does not end in an unconditional
    # transfer runs straight on into whatever label follows, and the
    # trimming above hides that by design — it removes the next
    # routine's comment header, which is the only visual cue.
    #
    # This has hidden the load-bearing detail twice. LAFFC_30 ends
    # `LD (LAA7B),HL` and falls into LB1C3, which UNDOES the position
    # snap it just recorded; current_level_2up_copier ends `JR NZ` on
    # its copy loop and falls into players_swap, which is the entire
    # turn alternation in 2-player mode. Both times the printed output
    # looked like a complete routine.
    last = ""
    for line in reversed(body):
        t = line.split(";")[0].strip()
        if t and not LABEL.match(line):
            last = t.upper()
            break
    unconditional = (last == "RET"
                     or (last.startswith(("JP ", "JR "))
                         and "," not in last))
    if last and not unconditional and not last.startswith("DEFB") \
            and not last.startswith("DEFW"):
        nxt = None
        for j in range(end, len(lines)):
            m = LABEL.match(lines[j])
            if m:
                nxt = m.group(1)
                break
        note = f" into `{nxt}`" if nxt else ""
        print(f"\n; --- FALLS THROUGH{note} ---")
        print(f"; Last instruction is `{last}`, which is not an "
              f"unconditional RET/JP/JR,")
        print(f"; so execution continues past the end of this listing. "
              f"Read on before")
        print(f"; concluding what the routine does.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
