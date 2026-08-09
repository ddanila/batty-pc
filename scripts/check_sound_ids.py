#!/usr/bin/env python3
"""The port's sound ids are the original's play_sounds_list positions.

`play_selected_sound` indexes a table of routine addresses with the
slot's first byte:

    play_selected_sound:
      LD HL,play_sounds_list-2 / ADD A,A / ... / JP (HL)

so an id IS a position in `play_sounds_list`, and getting one wrong
plays a different effect — or, past the end of the table, jumps into
whatever follows it.

This gate reads the table out of `original/disasm/routines/sound.asm`
and checks each `SND_*` constant in `src/sound.h` against the position
of its namesake routine. Only entries the disassembly NAMES are matched;
`play_sound_05`, `_06` and `_08` are unnamed there, so the port's names
for them (`SND_ALIEN_BLAST`, `SND_SPARK_FANOUT`) are its own and cannot
be checked against anything.

### SND_MAGNET is not in the table, on purpose

`play_sound_magnet` exists but is not queued: `magnets.asm` ends its
draw with a plain `CALL play_sound_magnet`, synchronously. The table's
`0D` slot is commented out in the disassembly as "Неиспользуемый звук".

The port routes it through its own queue as id `$0D`, which is a port
convention rather than a match — and `src/sound.h` claimed the ids
"match the original's play_sounds_list" without noting the exception.
This gate pins the exception so the claim stays honest: `$0D` must be
BEYOND the table, not inside it.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASM = ROOT / "original/disasm/routines/sound.asm"
HDR = ROOT / "src/sound.h"


def main() -> int:
    asm = ASM.read_text()
    m = re.search(r"play_sounds_list:\n((?:\s*DEFW[^\n]*\n)+)", asm)
    if not m:
        raise SystemExit("FAIL: play_sounds_list is not in "
                         f"{ASM.name} — if the table moved, point this "
                         f"gate at it")
    table = re.findall(r"DEFW\s+(\w+)", m.group(1))
    if not table:
        raise SystemExit("FAIL: play_sounds_list parsed to no entries")
    # Entry n (1-based) is id n.
    pos = dict((name, i + 1) for i, name in enumerate(table))

    hdr = HDR.read_text()
    consts = dict((n, int(v, 16)) for n, v in
                  re.findall(r"const u8 (SND_\w+)\s*=\s*0x([0-9A-Fa-f]+);", hdr))
    if not consts:
        raise SystemExit("FAIL: no SND_* constants in src/sound.h")

    # The port's names for the routines the disassembly does name.
    NAMED = {
        "SND_NORMAL_BRIK":  "play_sound_normall_brik",
        "SND_BAT_BEAT":     "play_sound_bat_beat",
        "SND_BALL_START":   "play_sound_ball_start",
        "SND_LIVE_ADD":     "play_sound_live_add",
        "SND_BAT_RESIZE_1": "play_sound_bat_resize_1",
        "SND_TRIPLE_BALL":  "play_sound_triple_ball",
        "SND_SHOT":         "play_sound_shot",
        "SND_BAT_RESIZE_2": "play_sound_bat_resize_2",
    }
    checked = 0
    for const, routine in sorted(NAMED.items()):
        if const not in consts:
            raise SystemExit(f"FAIL: {const} is gone from src/sound.h")
        if routine not in pos:
            raise SystemExit(f"FAIL: {routine} is not in play_sounds_list; "
                             f"the table changed")
        if consts[const] != pos[routine]:
            raise SystemExit(
                f"FAIL: {const} is ${consts[const]:02X} but {routine} sits at "
                f"position ${pos[routine]:02X} in play_sounds_list. "
                f"play_selected_sound indexes that table with the id, so this "
                f"plays the wrong effect.")
        checked += 1
    print(f"PASS sound_ids_match_table: {checked} named effects sit at their "
          f"play_sounds_list positions")

    # The magnet is played synchronously in the original and has no
    # table slot; the port queues it with an id past the end.
    if "SND_MAGNET" in consts and consts["SND_MAGNET"] <= len(table):
        raise SystemExit(
            f"FAIL: SND_MAGNET is ${consts['SND_MAGNET']:02X}, which is "
            f"INSIDE play_sounds_list (1..${len(table):02X}) — it would "
            f"collide with {table[consts['SND_MAGNET'] - 1]}. The original "
            f"calls play_sound_magnet directly from magnets.asm and never "
            f"queues it; the port's id must stay past the table.")
    print(f"PASS sound_magnet_outside_table: SND_MAGNET is past the "
          f"{len(table)} queued effects, as a port-only id")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
