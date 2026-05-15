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
