# Modded Batty for GT level captures

We assemble a patched copy of the original Batty source to produce a custom
`batty.sna` that boots straight into the level-init flow with all gameplay
overlays removed. This replaces the prior tape-boot-per-level approach
(~30 s/level) with a single-emulator capture loop (~10 s for all 15).

## Why

The earlier capture pipelines all suffered from one of:

- **State pollution**: snap-load + hard_reset inside one emulator session
  doesn't fully reset hardware state; per-iteration changes accumulate and
  destroy sprite animation (magnets degraded into fuzzy sparkles by L4+).
- **Settle-time fragility**: with the original game running, the gameplay
  loop runs during the wall-clock settle, eventually causing the ball to
  auto-launch / aliens to spawn / bat-handler to draw — every captured
  frame is dependent on how many gameplay iterations sneaked in.
- **HUD noise**: scores, "1UP HI 2UP" labels, lives indicator all end up
  in the captures — irrelevant for visual parity testing.

Modifying the source removes the variables instead of working around them.

## Where the sources live

The official disassembly from `github.com/CityAceE/Batty` is added as a
git submodule at `original/disasm/`. **Never modify files inside the
submodule.** All edits happen in a build-time copy.

`scripts/build_modded_batty.py` is the single source of truth for what we
change. It copies `original/disasm/*` to `build/modded_batty/`, applies
line-number-targeted patches to `batty.asm`, then invokes `sjasmplus`
(built from source at `~/fun/sjasmplus/`).

## Patches applied

Each entry below is keyed by 1-based line number in the upstream
`batty.asm`. The build script asserts a substring match before patching,
so if upstream renumbers, the build fails loudly.

| Line  | What                                                            | Why                                                                                  |
| ----- | --------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| 572   | `XOR A` (start of `scr_score_update`)         → `RET`           | Per-frame score redraw is a no-op — score digits never reach VRAM                    |
| 5966  | `LD A,$83` (all_var_init final write to bat+$14) → `LD A,$09`   | bat+$14 = `$09` = "kill-aliens" bonus flag; `enemy_prepare` early-returns → no bird  |
| 6094  | `CALL disp_main_menu_and_wait_keys`           → 3-byte NOP      | Skip title screen entirely — boot goes straight into `game_restart` body             |
| 6145  | `CALL show_window_round_number`               → 3-byte NOP      | No "PLAYER N / ROUND NN" banner                                                      |
| 6147  | `CALL pause_long`                             → 3-byte NOP      | No 1.2s pause that originally let players read the banner                            |
| 6148  | `CALL all_metal_briks_animation_snd`          → 3-byte NOP      | Metal-brick fade-in skipped (~40 ms/brick saved)                                     |
| 6261  | `CALL restore_objs_and_magnet`                → `DEFB $18, $FE, $00` | `JR $` + NOP — spins AFTER the first gameplay-loop iter has painted bat/ball/lives into scr_buff and flushed to VRAM. The trap fires right before the restore step that would wipe them back. |
| LBE8B_11 region | 3 × `CALL print_obj_to_buff` (spr_1up/2up/hi) + 3 × `CALL print_score_in_game` → 3-byte NOPs | No "1UP HI 2UP" labels, no score digits in HUD |

The L6853 lives-indicator skip is **not** applied — keeping it would
silently exclude `render_lives` from the GT and mask any drift. To
re-extract `frame_l1.bin` without lives baked in, temporarily re-add
the L6853 patch, run `scripts/extract_frame.py`, then remove again
before recapturing GTs (see comment in `build_modded_batty.py`).

The `JR $` spin at line 6261 (= **0xBB61**) is critical: it leaves the
CPU in the "running" state (so `set_register PC=…` can break it out for
the next level), and runs **exactly one** gameplay-loop iteration before
catching — long enough for `print_obj_from_buf_to_scr` to flush the bat
+ ball + lives indicator into VRAM, short enough that
`restore_objs_and_magnet` hasn't wiped them again. The GT therefore
captures the post-paint frame with all objects drawn.

We picked `JR $` over `DI; HALT` because once a Z80 enters halt state
with interrupts disabled, only an NMI or reset can resume it; setting PC
via ZRCP does not. Spin-loop is functionally equivalent for our purposes.

## Driver flow (`scripts/capture_levels_modded.py`)

```
boot ZEsarUX with build/modded_batty/batty.sna
sleep 2s                                       # game_start -> game_restart -> L1 init -> first paint -> spin at BB61
snap level_01.scr

for n in 2..15:
  poke $B7EA = n-1                             # current_level_number_1up
  set PC = 0xBA24                              # = CALL briks_calc, restarts the level-init flow
  resume CPU
  poll get_registers until PC == 0xBB61        # spin reached again
  snap level_<n>.scr
```

`briks_calc` re-reads the level table; `all_var_init` (run inside
`LB9E8_1`) resets the 11-slot object table; `game_screen_draw_to_buffer`
repaints the entire screen for the new level. So each iteration after the
first gets a fully fresh paint — no state pollution.

End-to-end the script captures and renders all 15 levels in under 10
seconds on this machine, vs ~8 min for the tape-boot-per-level pipeline.

## Adding new modifications

If you need another behaviour change (e.g. force a specific level's
magnet to be drawn at a particular frame, blank the frame ornament, etc):

1. Find the address / source line in `original/disasm/batty.asm`.
2. Add a `(line_no, needle, replacement)` entry to `PATCHES` in
   `scripts/build_modded_batty.py`. Use `DEFB $hh, $hh, $hh` if you need
   to keep byte-count alignment with the original instruction (every
   3-byte `CALL` → `DEFB $00,$00,$00`; every 2-byte `JR cc,…` → `JR …`).
3. `make` (or `python3 scripts/build_modded_batty.py`), then `python3
   scripts/capture_levels_modded.py`.
4. Verify the rendered GT looks correct.

Do **not** edit `original/disasm/` directly. The submodule must remain
clean so we can `git diff` upstream changes and re-apply our patches.
