#!/usr/bin/env python3
"""Build a modified Batty.sna that auto-runs into the level-init flow with
HUD elements removed, no banner, and aliens suppressed.

The original sources live in the read-only submodule at original/disasm/.
This script copies them to build/modded_batty/, applies in-place patches
to batty.asm, then invokes sjasmplus to produce build/modded_batty/batty.sna.

Patches applied (line numbers reference the unmodified source):
  L5965   `LD A,$83`                         -> `LD A,$09`
          (all_var_init's final write to object_bat_1+$14; $09 = kill-aliens
          power-up flag, so enemy_prepare early-returns and no bird spawns)
  L6094   `CALL disp_main_menu_and_wait_keys` -> 3 NOPs
          (skip the title-screen menu; game enters game_restart -> level-init
          directly on boot)
  L6145   `CALL show_window_round_number`    -> 3 NOPs
          (no ROUND-N banner overlay)
  L6857   `JR Z,LBE8B_10`                    -> `JR LBE8B_10`
          (always skip lives-indicator drawing)
  L6881   LBE8B_11 body: 3 `CALL print_obj_to_buff` for spr_1up/2up/hi
          plus 3 `CALL print_score_in_game`  -> 3-NOP runs each
          (no "1UP HI 2UP" labels, no score digits)
  After level-init, an external driver pokes the level counter + sets
  PC=0xBA24 to re-run for each level, then snaps the screen via ZRCP.
"""
import shutil
import subprocess
import sys
from pathlib import Path

ROOT          = Path(__file__).resolve().parent.parent
SRC_DIR       = ROOT / 'original' / 'disasm'
BUILD_DIR     = ROOT / 'build' / 'modded_batty'
SJASMPLUS     = Path.home() / 'fun' / 'sjasmplus' / 'sjasmplus'
ASM_FILE      = 'batty.asm'

# Each entry: (1-based line number, must-contain substring, replacement line).
# The substring is asserted to be present on the targeted line so the script
# fails loudly if upstream renumbers things.
PATCHES = [
    (572,
     'XOR A',
     '  RET        ; MOD: short-circuit scr_score_update so per-frame score redraw does nothing'),
    (5966,
     'LD A,$83',
     '  LD A,$09   ; MOD: $83 -> $09 sets object_bat_1+$14 to "kill-aliens" bonus'),
    (6094,
     'CALL disp_main_menu_and_wait_keys',
     '  DEFB $00, $00, $00   ; MOD: NOP "CALL disp_main_menu_and_wait_keys"'),
    (6145,
     'CALL show_window_round_number',
     '  DEFB $00, $00, $00   ; MOD: NOP "CALL show_window_round_number"'),
    (6147,
     'CALL pause_long',
     '  DEFB $00, $00, $00   ; MOD: NOP "CALL pause_long" (skip 1.2s pause)'),
    (6148,
     'CALL all_metal_briks_animation_snd',
     '  DEFB $00, $00, $00   ; MOD: NOP "CALL all_metal_briks_animation_snd"'),
    # (6853 lives-indicator skip was removed — keeping it would NOP the
    # original's lives draw and silently exclude it from the GT, which
    # masks any drift in our `render_lives`. See state4-bat-band-triage.md.)
    # Spin AFTER the first gameplay-loop iteration has painted bat/ball/
    # lives into scr_buff and flushed them to VRAM (print_obj_from_buf_to_scr
    # at the start of LBAED_5). We trap on the very next instruction —
    # `CALL restore_objs_and_magnet` (3 bytes) — which is what would
    # normally wipe those objects back to bg before the next frame.
    # Result: GT shows the post-paint frame, bat/ball/lives included.
    (6261,
     'CALL restore_objs_and_magnet',
     '  DEFB $18, $FE, $00    ; MOD: JR $ + NOP - spin AFTER first paint+flush (bat/ball/lives in VRAM)'),
]

# 3-byte NOP replacements (each CALL is 3 bytes; we keep the surrounding
# IX setup so byte positions of subsequent labels don't shift).
NOP_CALLS_BY_LINE = []   # we identify them by content+nearby line range

# LBE8B_11 (score labels) + score digit prints — three CALL print_obj_to_buff
# and three CALL print_score_in_game. We find these by scanning the file
# around LBE8B_11.
def patch_score_block(lines):
    """In the LBE8B_11 region, NOP-out the print_obj_to_buff and
    print_score_in_game CALLs. Returns the modified lines list."""
    in_region = False
    out = []
    nopped = 0
    for idx, line in enumerate(lines, start=1):
        if not in_region and line.strip().startswith('LBE8B_11:'):
            in_region = True
        if in_region:
            stripped = line.strip()
            # Stop at the print_magnets call (start of next functional block).
            if stripped.startswith('CALL print_magnets'):
                in_region = False
                out.append(line)
                continue
            if stripped.startswith('CALL print_obj_to_buff') or \
               stripped.startswith('CALL print_score_in_game'):
                # Preserve indent for cleanliness.
                indent = line[:len(line) - len(line.lstrip())]
                out.append(f'{indent}DEFB $00, $00, $00   ; MOD: NOP "{stripped}"\n')
                nopped += 1
                continue
        out.append(line)
    return out, nopped


def apply_patches(asm_path):
    text = asm_path.read_text()
    lines = text.splitlines(keepends=True)
    for line_no, needle, replacement in PATCHES:
        original = lines[line_no - 1]
        if needle not in original:
            raise SystemExit(
                f'patch failed at line {line_no}: expected "{needle}", '
                f'got: {original.rstrip()!r}'
            )
        lines[line_no - 1] = replacement.rstrip('\n') + '\n'
        print(f'  patched L{line_no}: {needle}')
    lines, nopped = patch_score_block(lines)
    print(f'  NOPed {nopped} score-block CALLs in LBE8B_11')
    asm_path.write_text(''.join(lines))


def main():
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    print(f'copying {SRC_DIR} -> {BUILD_DIR}')
    shutil.copytree(SRC_DIR, BUILD_DIR, ignore=shutil.ignore_patterns('.git', '.git*'))

    asm = BUILD_DIR / ASM_FILE
    print(f'applying patches to {asm}')
    apply_patches(asm)

    print(f'invoking sjasmplus...')
    res = subprocess.run([str(SJASMPLUS), '--lst=batty.lst', ASM_FILE],
                         cwd=BUILD_DIR,
                         capture_output=True, text=True)
    sys.stdout.write(res.stdout)
    sys.stderr.write(res.stderr)
    if res.returncode != 0:
        raise SystemExit(f'sjasmplus failed (exit {res.returncode})')

    sna = BUILD_DIR / 'batty.sna'
    print(f'\nproduced: {sna} ({sna.stat().st_size} bytes)')


if __name__ == '__main__':
    main()
