#!/usr/bin/env python3
"""FAIL-gated state4 sweep across all 15 levels.

`make test` fail-gates state4 only on the default L1; the other 14
levels were INFO-only, which let the L3/L9 alien-race artefact sit
unnoticed behind a stale "15/15 pixel-perfect" claim (the lessons.md
"INFO is for accepted drift, not unmeasured surface" trap — see
notes/per-level-profile.md, 2026-06-11). This gate boots every level
through the regular `BATTY_LEVEL=N make test` flow and FAILS on any
state4 deviation, so per-level drift can't hide again.

Slow (15 QEMU boots, ~10 min) — wired into parity-check-full, not the
fast core.

    make test-levels-sweep
"""
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import test_floppy

FLOPPY = Path(test_floppy())
N_LEVELS = 15


def main():
    fails = 0
    for n in range(1, N_LEVELS + 1):
        FLOPPY.unlink(missing_ok=True)
        proc = subprocess.run(f'BATTY_LEVEL={n} make test', shell=True,
                              capture_output=True, text=True)
        m = re.search(r'(PASS|INFO|FAIL) state4_level1: (.*)', proc.stdout)
        line = m.group(0) if m else '(no state4 line — boot/capture failed)'
        ok = bool(m) and m.group(1) == 'PASS'
        print(f'  L{n:02d}: {line}  [{"PASS" if ok else "FAIL"}]')
        if not ok:
            fails += 1
    if fails == 0:
        print(f'PASS levels_sweep: state4 pixel-identical on all {N_LEVELS} levels')
    else:
        print(f'FAIL levels_sweep: {fails}/{N_LEVELS} levels deviate from GT')
    return fails


if __name__ == '__main__':
    sys.exit(main())
