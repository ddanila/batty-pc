# Lessons learned

Accumulated wisdom from past iterations on this project. Each entry is a
specific class of mistake to watch for — readable in isolation, with
enough "why" that the rule survives without its incident.

## Audit post-processing when changing pipelines

**Rule.** When you change how data is acquired, every post-processing
step that "cleaned up" assumptions of the old pipeline is suspect.
Re-justify each one against the new pipeline's properties before
re-running.

**Why.** During the Batty GT capture work we burned a full session
iterating on snap-load / hard_reset / fresh-emulator-per-level / NOP-
this-and-that to fix "missing magnets on L4". Every iteration produced
the same "magnet missing" output. The actual cause was
`scripts/clean_gts.py`, a leftover from the original snap3-based
pipeline. It wiped pixel bytes in y=120..160 (mid-playfield band) to
clear snap3's mid-game debris. Once we switched to fresh-tape-boot-per-
level captures, that band was clean and contained the magnet sprite —
but clean_gts still ran in the workflow and erased it. The capture
script had been correct the whole time; the renderer was looking at
post-processed `.scr` files that had been wiped.

**How to apply.** Whenever you rip out an upstream stage and replace
it, list every downstream "cleanup" / "normalize" / "fixup" step and
ask: *was this step compensating for a defect that no longer exists?*
If yes, remove it before debugging output mismatches. Symptoms that
should trigger this audit:

- "The captures look right standalone but wrong after the workflow runs"
- Output that differs depending on whether you run a single step or the
  whole chain
- Post-step diagnostics showing data that was present pre-step is gone

When introducing a new pipeline stage, ship it as a separate PR that
*removes* the now-obsolete cleanup rather than running both side-by-side.

## Trapping a watchpoint doesn't tell you what the routine does

**Rule.** A VRAM-watchpoint hit gives you a PC, not a purpose. If two
routines share an inner blitter, your trap may land in the wrong one.
Confirm the routine's role by tracing where it's *called from*, not
just what it writes to.

**Why.** We watched writes to `0xE800` during level-1 startup and
trapped at `0xAD8F`. We named it "the brick-field blitter" in
`notes/sprites.md` (Phase A1) and built a whole narrative around it:
multi-pass compositor, sprite cache as pre-shifted bricks, "days of
Z80 RE" to port. When we finally read the named disasm, `0xAD8F` was
`all_metal_briks_frame` — the metal-brick *shimmer animation*, called
once-per-N-frames from a side path, not the level paint at all. The
actual level paint (`print_briks` at `0xADE1`) shares the same inner
8-row × 2-byte blit, which is why the trap looked plausible.

The mistake cost us ~3.5 KB of "sprite cache" theorising and a
shortcut-bypass for L1 (shipping `brick_bitmaps.bin`) when the real
routine was ~30 lines we could port directly. The disasm replacement
of "Days" with "hours" was almost entirely this single misread.

**How to apply.** When a watchpoint traps, before naming the routine:

- Look at the call site — what *caller* enters this PC? Is it a
  level-init path, a per-frame path, or a periodic animation? The
  named disasm or even a cycle counter on the watchpoint hits will
  tell you.
- Check the routine's neighbours in the same file: an *animation*
  driver often sits next to a *paint* driver with a similar inner
  blit. Don't assume the trap landed in the one you expected.
- When two routines share inner code, name the inner code by its
  shape (`blit_16x8`) and the outer drivers by their *purpose*
  (`paint_bricks` vs `animate_metal_bricks`). Avoid letting the
  outer name leak from a wrong assumption about what called it.

Symptoms that should trigger this audit:

- The routine you trapped is much simpler than the behaviour it's
  supposed to drive.
- The "cache" it reads from is mostly populated with one obvious
  asset class (here: bricks) and the rest is dark — that's an
  animation cache, not a sprite library.
- You're inventing multi-pass / SMC / sentinel-pointer machinery to
  explain how this routine produces the observed pixels. The real
  routine probably doesn't.
