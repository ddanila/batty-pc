#!/usr/bin/env python3
"""Key 0 in the menu starts the game; nothing else does.

The original's `main_menu` tail is

    L93F8_10:
      LD A,$EF / CALL in_a_fe / AND $01 / RET NZ
      JP L93F8_0

and `in_a_fe` CPLs the port read, so bits are active HIGH after it and
`RET NZ` means "key 0 IS pressed -> leave the menu and start the game".
Key 0 is the only way out; every other key falls back into the poll
loop. Until 2026-08-09 the port returned ST_HISCORE for `0 / ENTER /
other` with a comment saying it "would start a game".

### Why this is a source gate

The state the menu returns is not something a screendump can see
directly, and the one gate that walks these screens — `test-visual` —
walks them with ENTER, which deliberately still advances the attract
chain. So `test-visual` looks identical whether key 0 starts a game or
not: it never presses 0.

What this pins:

  - `run_menu` returns ST_LEVEL for '0', and for nothing else
  - ENTER still returns ST_HISCORE, because test-visual's title ->
    menu -> hi-score -> level walk depends on it. Making ENTER start the
    game would silently turn that gate's hi-score checkpoint into a
    level capture, and it would still "pass" — it diffs each checkpoint
    against its own reference, so the failure would read as a rendering
    regression on the wrong screen.
  - an unrecognised key falls through to the poll loop rather than
    leaving the menu
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "main.cpp"


def body_of(src: str, signature: str) -> str:
    start = src.index(signature)
    depth = 0
    i = src.index("{", start)
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i:j + 1]
    raise SystemExit(f"FAIL: could not find the end of {signature}")


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def main() -> int:
    src = SRC.read_text()
    try:
        body = strip_comments(body_of(src, "static state_t run_menu(void) {"))
    except ValueError:
        raise SystemExit("FAIL: run_menu is gone; if the menu moved, point "
                         "this gate at its new home")

    compact = "".join(body.split())

    if "k=='0'" not in compact or "returnST_LEVEL" not in compact:
        raise SystemExit(
            "FAIL: the menu no longer starts a game on key 0. The original "
            "leaves main_menu only via `AND $01 / RET NZ` on the $EFFE row "
            "(in_a_fe CPLs, so that is 'key 0 pressed'). PLAN.md WS1.")
    print("PASS menu_start_on_zero: key 0 returns ST_LEVEL")

    # ENTER must still advance the attract chain — test-visual walks
    # title -> menu -> hi-score -> level with it.
    if "returnST_HISCORE" not in compact:
        raise SystemExit(
            "FAIL: the menu no longer advances to the hi-score screen. "
            "test-visual sends ENTER at each state and diffs each "
            "checkpoint against its OWN reference, so if ENTER started a "
            "game instead, its state3_hiscore would capture a level and "
            "the failure would read as a rendering regression.")
    print("PASS menu_enter_advances: ENTER still returns ST_HISCORE")

    # ST_LEVEL must be reachable ONLY from the key-0 branch. Any other
    # `return ST_LEVEL` in run_menu would be a second way out.
    if compact.count("returnST_LEVEL") != 1:
        raise SystemExit(
            f"FAIL: run_menu has {compact.count('returnST_LEVEL')} paths to "
            "ST_LEVEL; the original has exactly one, the key-0 test")
    print("PASS menu_single_start_path: exactly one route out to ST_LEVEL")

    # An unrecognised key must fall back into the poll loop. The check
    # that matters is what the key-handling block ENDS with: several
    # `continue;` already sit in the A/B/1-3 branches, so merely finding
    # one proves nothing. The last statement is the fallthrough.
    kb = body_of(body, "if (kbhit()) {")
    tail = strip_comments(kb).rstrip()
    assert tail.endswith("}")
    last = tail[:-1].rstrip().split(";")[-2].strip() + ";"
    if last != "continue;":
        raise SystemExit(
            f"FAIL: an unrecognised key ends the menu with `{last}` instead "
            "of falling back into the poll loop. The original's tail is "
            "`JP L93F8_0` — every key that is not 0 re-polls.")
    print("PASS menu_ignores_other_keys: unrecognised keys re-poll")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
