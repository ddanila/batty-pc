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

---

Resolved history:

(bug #3 "multi-hit bricks shimmer continuously" resolved 2026-06-11:
`step_brick_hit_anim` now mirrors the original's `(c+1) & $0F` literally —
the wrap to 0 frees the slot, one ~15-tick pass; the spawn dedupe
invention was dropped (LAFFC_35 takes the first free slot). Full decode +
fix in `notes/metal-shimmer.md`, "FIXED 2026-06-11". Gates: parity-check,
test-brick-flash, test-frame-step all green at the documented floor.)

(the `replay-l3-entry` "0 → 1885 px" entry was resolved
2026-06-11: the SCREEN had already healed back to 0/23040 px by the time
it was re-triaged, and the residual FAIL was the gate comparing
moment-dependent probe rows across different instants — the original
probed at the `$BA83` pause vs the port's PROBE.TXT rewritten by its ESC
handler at harness teardown. The "original=8E49" RNG reading was an echo
of the setup pin at the WRONG address `$8E17` (real `random_number` is
`$8D48` = 3793). Gate semantics fixed + green; full story in
`notes/replay-harness.md`, "Gate semantics corrected".)
