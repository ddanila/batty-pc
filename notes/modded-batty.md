# Modded Batty, for the GT level captures

`scripts/build_modded_batty.py` assembles a patched copy of the original
source into a `batty.sna` that boots straight into the level-init flow with
every gameplay overlay removed. `scripts/capture_levels_modded.py` then
captures all 15 level GTs in under 10 seconds, against ~8 minutes for the old
tape-boot-per-level pipeline.

## Why patch the source instead of working around it

The earlier capture pipelines each suffered from one of:

- **State pollution.** Snap-load plus `hard_reset` inside one emulator session
  does not fully reset hardware state, so per-iteration changes accumulate and
  destroy sprite animation — magnets degraded into fuzzy sparkles by L4.
- **Settle-time fragility.** With the game running, the gameplay loop runs
  during the wall-clock settle, eventually auto-launching the ball, spawning
  aliens or redrawing the bat. Every captured frame depends on how many
  gameplay iterations sneaked in.
- **HUD noise.** Scores, the "1UP HI 2UP" labels and the lives indicator all
  end up in the captures, and none of them is what a visual parity test is
  measuring.

Modifying the source removes the variables rather than tolerating them.

## Where the sources live

The disassembly from `github.com/CityAceE/Batty` is a submodule at
`original/disasm/`. **Never modify files inside the submodule** — it must stay
clean so upstream changes can be diffed and the patches re-applied. The build
script copies it to `build/modded_batty/`, applies line-targeted patches to
`batty.asm`, then invokes `sjasmplus`.

## The patches

Keyed by 1-based line number in the upstream `batty.asm`. The script asserts a
substring match before patching, so if upstream renumbers, the build fails
loudly rather than patching the wrong line.

| line | change | why |
|------|--------|-----|
| 572 | `XOR A` (start of `scr_score_update`) → `RET` | the per-frame score redraw becomes a no-op, so digits never reach VRAM |
| 5966 | `LD A,$83` (final `all_var_init` write to bat+$14) → `LD A,$09` | `$09` is the kill-aliens bonus flag, so `enemy_prepare` early-returns and no bird appears |
| 6094 | `CALL disp_main_menu_and_wait_keys` → 3-byte NOP | skip the title screen; boot goes straight into `game_restart` |
| 6145 | `CALL show_window_round_number` → NOP | no PLAYER/ROUND banner |
| 6147 | `CALL pause_long` → NOP | no 1.2 s pause to read it |
| 6148 | `CALL all_metal_briks_animation_snd` → NOP | skip the metal-brick fade-in |
| 6261 | `CALL restore_objs_and_magnet` → `DEFB $18,$FE,$00` | `JR $` + NOP: spin AFTER the first gameplay iteration has painted and flushed |
| `LBE8B_11` region | 3x `CALL print_obj_to_buff` (1up/2up/hi) + 3x `CALL print_score_in_game` → NOPs | no HUD labels, no score digits |

Use `DEFB $hh, $hh, $hh` whenever byte-count alignment matters — every 3-byte
`CALL` becomes three bytes, every 2-byte `JR cc,…` becomes a 2-byte `JR`.

**The `JR $` spin at line 6261 (= `0xBB61`) is the load-bearing one.** It
leaves the CPU in the "running" state, so `set_register PC=…` can break it out
for the next level, and it runs **exactly one** gameplay-loop iteration before
catching — long enough for `print_obj_from_buf_to_scr` to flush the bat, ball
and lives indicator into VRAM, short enough that `restore_objs_and_magnet` has
not wiped them again. So the GT captures the post-paint frame with every
object drawn.

`JR $` rather than `DI; HALT`: once a Z80 halts with interrupts disabled, only
an NMI or a reset resumes it, and setting PC over ZRCP does not.

**The lives-indicator skip is deliberately NOT applied.** Keeping it would
silently exclude `render_lives` from the GT and mask any drift there. To
re-extract `frame_l1.bin` without lives baked in, temporarily re-add that
patch, run `scripts/extract_frame.py`, then remove it again before
recapturing GTs.

## The capture loop

```
boot ZEsarUX with build/modded_batty/batty.sna
sleep 2s                              # to the first paint, spinning at BB61
snap level_01.scr

for n in 2..15:
  poke $B7EA = n-1                    # current_level_number_1up
  set PC = 0xBA24                     # = CALL briks_calc, restarts level init
  resume CPU
  poll get_registers until PC == 0xBB61
  snap level_<n>.scr
```

`briks_calc` re-reads the level table, `all_var_init` (inside `LB9E8_1`)
resets the 11-slot object table, and `game_screen_draw_to_buffer` repaints the
whole screen, so every iteration after the first gets a fully fresh paint with
no state carried over. The same poke-`$B7EA`+`$BA24` recipe is what
generalises the byte-exact oracle to any level (`notes/replay-harness.md`).

## Adding a modification

1. Find the address or source line in `original/disasm/batty.asm`.
2. Add a `(line_no, needle, replacement)` entry to `PATCHES` in
   `scripts/build_modded_batty.py`.
3. Rebuild and re-run the capture.
4. Verify the rendered GT looks correct.

**Audit post-processing when you change this pipeline.** A leftover
`clean_gts.py` once wiped the y=120..160 band — including magnet pixels — from
the GTs, and cost a session of "missing magnets" debugging on a capture that
had got them right.
