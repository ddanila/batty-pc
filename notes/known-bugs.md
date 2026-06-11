# Known bugs (user-reported, unfixed)

Concrete defects observed by the user that aren't yet caught by the
visual-regression test (= mid-game state we don't snapshot). Listed
here so the next iter has a target. When fixing, add a section to
`per-level-profile.md` or the relevant area doc; remove from here.

## 1. Background glitches / leftovers after a brick is destroyed (2026-06-11)

Small stray pixels remain on the background where a brick used to be,
after it gets destroyed. Likely suspects (unverified): the dirty-rect
union not covering the full footprint of the destroy-time effects —
`brick_flash_spawn` / `brick_hit_anim` draw into `scr_buff` over the
brick cell, while `mark_brick_row_dirty` (src/main.c:3008) only scopes
the band-cache rebuild to the brick's own 8-px row; anything the flash
or shimmer touched outside that row/byte range never gets restored from
`bg_scr_buff` by `restore_prev_dirty_from_static_cache`
(src/main.c:3019).

## 2. Inverted-colour leftovers after aliens fly over bricks (2026-06-11)

When an enemy passes over the brick area, it leaves residue behind with
the colour INVERTED. The inversion strongly implicates the XOR shadow
blit case: in `blit_masked_to_scr_buff_ptr` (src/main.c:~1568),
`mask=0, pix=1` TOGGLES the background bit (`scr_buff' = (mask|scr) ^
pix`, the dotted bat-shadow effect). If the enemy's dirty byte-range for
that frame doesn't include the shadow columns (mask=0 bytes may sit
outside whatever extent gets recorded into `prev_dirty_min/max_byte`),
the toggled bits are never restored from the static cache and stay
flipped — i.e. inverted background. Check what extent the enemy draw
path records vs the sprite's full (mask+shadow) footprint.

## 3. Multi-hit ("red") bricks shimmer continuously instead of animating once (2026-06-11)

Hitting a two-hit brick the first time starts the hit animation and it
then plays FOREVER (until the brick is destroyed), instead of playing a
single pass. This is currently *by design* in the port:
`step_brick_hit_anim` (src/main.c:6510) frees a slot only when the
cell's bit 7 (destroyed) is set, and cycles the frame counter
`(c+1) & $0F` forever, with a comment claiming this ports
metal_brik_anim ($B6A9) — "an undestructible (metal) brick shimmers
PERMANENTLY once hit; a multi-hit brick shimmers until its final hit".
VERIFIED against the original disasm (2026-06-11,
`original/disasm/batty.asm`): the port comment is WRONG, for BOTH brick
kinds. The slot counter at `briks_data` IX+$00 doubles as the
free/active flag: spawn (`LAFFC_36`) takes a slot with counter==0 and
`INC`s it to 1; `fill_briks_data` ($B694) skips counter==0 slots
(`AND A / CALL NZ,metal_brik_anim`); `metal_brik_anim` ($B6A9) ends with
`INC A / AND $0F`, so the counter runs 1..15 and `(15+1)&$0F = 0` FREES
the slot. One ~15-tick pass (8 frames x 2 ticks), then it stops —
metal and multi-hit alike; the `BIT 7,(HL)` check only frees the slot
early if the brick is destroyed mid-pass. Two implications for the fix
in `step_brick_hit_anim` (src/main.c:6510):
  1. expire the slot after one pass instead of wrapping back to tick 1;
  2. the original draws each frame into BOTH screen and buffer before
     incrementing, so the LAST anim frame stays persisted on the
     surviving brick (the "hit" half-state look) — don't restore the
     pristine brick pixels when the anim ends.
Also note the original does NOT dedupe by brick: re-hitting the same
brick mid-anim takes a SECOND free slot (LAFFC_35 only looks for
counter==0), unlike the port's restart-existing-slot logic in
`brick_hit_anim_spawn` (src/main.c:6490).

---

Resolved history:

(the `replay-l3-entry` "0 → 1885 px" entry was resolved
2026-06-11: the SCREEN had already healed back to 0/23040 px by the time
it was re-triaged, and the residual FAIL was the gate comparing
moment-dependent probe rows across different instants — the original
probed at the `$BA83` pause vs the port's PROBE.TXT rewritten by its ESC
handler at harness teardown. The "original=8E49" RNG reading was an echo
of the setup pin at the WRONG address `$8E17` (real `random_number` is
`$8D48` = 3793). Gate semantics fixed + green; full story in
`notes/replay-harness.md`, "Gate semantics corrected".)
