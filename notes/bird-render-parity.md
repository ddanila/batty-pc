# Bird/UFO render parity — oracle triage (2026-06-12)

Follow-up to the "~21 px in-flight delta" (known-bugs.md): attributed it
against the original via a new aligned-alien harness, and found the real
gaps are in the ANIM DRIVER and the SPRITE DATA, not the dirty path.

## The harness (reusable)

`replays/l3-enemy-flyover.json` — derived from l3-brick-flash: after the
$BA83 alignment it additionally pokes `object_enemy` ($9B96) with the
fresh-spawn bird (x=168, y=1, dir=$10, spd=1, +$12=$F0, +$13=$70), the
same 22 bytes the port seeds via BATTY_REPLAY_ENEMY_OBJECT (the
FRESH_ENEMY of test_enemy_descend). With the steer-test env
(RANDOM=3793 SEED=962A COUNTER=2) both sides are in positional lockstep
through f24+: port f24 probe x=165 y=24 dir=$13 == original probe; the
original repicks its target ~f25-32 and diverges after (phase-gated).
Original-side runs:

    python3 scripts/capture_frame_timeline_original.py \
        --snapshot build/snapshots/20260513T202101Z.sna --frames 0,16,24 \
        --zesarux tools/zesarux/src/zesarux \
        --setup-from-replay replays/l3-enemy-flyover.json \
        --probe-ball 0x9B96 --out build/tl3_orig

Trick: `--probe-ball 0x9B95` (base−1) makes the probe's "x" field land
on sprite_num (+$01) — original f24 sprite_num = 4, same as the port.

## Findings (verified)

1. **The "21 px at f50" was the bomb/enemy DRAW ORDER** — RESOLVED
   2026-06-12 (second pass). The first version of this note blamed an
   "anim-phase A/B artifact" via a `(pit & 3)` gate — WRONG on
   re-reading: the anim counter is per-object (`misc_12++`), hence
   frame-deterministic (only the STEER gates on the global counter).
   The decisive measurements: f50 probes are byte-identical between the
   dirty and full boots (same enemy position AND sprite_num) yet 21 px
   differ; a third mode (BATTY_FORCE_BALL_FULL_REDRAW = full COMPOSE +
   dirty FLUSH) matches full-flush exactly → the bug was in the
   simple-path COMPOSE. The probe also showed `bomb_state=active01` at
   (75,42), overlapping the UFO at (82,44): the simple path drew
   enemy-then-bomb, the full path bomb-then-enemy — the 21 px were the
   overlap rendered with opposite stacking. The original paints objects
   in $9AD0 slot order (balls 1-3, bullets, bats, bonus/bomb/pts400
   shared slot $9B80, ENEMY $9B96, rocket $9BAC — later = on top), so
   bomb-under-enemy is correct. FIX: both compose paths reordered to
   the slot order (this also corrected bonus-vs-enemy and
   extra-balls-on-top-of-everything stacking in both paths). Gate:
   `make test-enemy-flyover-redraw` (the promoted repro, now 0 px;
   wired into parity-check-full). All dirty-redraw A/B gates +
   parity-check + test-frame-step green after the reorder.

2. **LAAD2 decode ($AAD2)** — the original's shared per-object anim
   stepper (bird, ufo, blast, bat, bullet): `+$12` is a cadence counter
   decremented by $40 per call; on underflow the sprite frame `+$01`
   advances LINEARLY and wraps via `+$13`'s nibbles (high = last frame,
   low = first frame), and `+$12` reloads as `((res<<2)&$C0)|res`. Bird:
   +$12=$F0 +$13=$70 → frames 0..7, 4 frames/step. UFO: +$12=$60
   +$13=$90 → frames 0..9 (the original `anim_ufo` table has TEN
   entries: 1,2,3,4,5,6,5,4,3,2 — the port's `spr_ufo_frames[8]` is
   missing the last two and wraps with `& 7`). Blast: kill_enemy seeds
   +$12=$50, handler forces +$13=$90, slot freed at sprite_num==9.
   Called unconditionally per frame once past the y<8 descend gate (the
   ufo handler's `LD A,(counter_misc)/AND $00/CALL Z` is an
   always-taken quirk). The bird handler's facing-mirror block
   (LAA02..: `$0E - n` / `(n^7)+7`) is DEAD CODE in the shipped binary —
   raw bytes confirm both results are discarded (the `LD A,(IX+$13) /
   LD (IX+$13),A` no-op looks like patched-out surgery), so the bird
   never mirrors. The port's "separate counter bumped on the global PIT
   phase" + 8-entry ping-pong tables is an invention that should be
   replaced by a literal LAAD2 port — that also makes the anim
   frame-deterministic (fixing finding 1 and making A/B harnesses
   stable).

3. **sprites.bin's enemy sprites differ from live memory.** The whole
   blob range differs from the in-game snapshot by ~3856 bytes — pix
   bytes only, masks identical, relation `live_pix = blob_pix XOR mask`.
   Where mask=0 the encodings agree (why most sprites render
   pixel-perfect), but e.g. spr_bird_3 has 37 differing bytes including
   mask≠0 rows 10..14 (an AA/55 XOR-shadow dither tail entirely absent
   from the blob's render) and ink/paper-inverted body rows. Rendering
   spr_bird_3 from live data with the original's `(mask|scr)^pix`
   produces a DIFFERENT image than the port's blit on the blob data.
   Open: whether the game transforms sprite pix at boot (blob captured
   pre-transform) or the extractor XOR'd them; either way the port's
   bird/ufo render does not match the original's live data pixel-wise.

4. **Unexplained "rings".** At f24 both sides show 7-px ring outlines
   flanking the bird (original: TWO per side at x≈137-151/192-206, rows
   21-28; port: one per side, slightly different x) — not present in
   the static GT (clean texture), not part of any 24-px-wide bird frame
   (all spr_bird_1..5 headers are w=3), outside the sprite's footprint.
   Something runtime draws/leaves them on both sides, differently. The
   38-px f24 port-vs-original diff is exactly these ring deltas. Needs
   its own decode (watch the original draw them — VRAM watchpoint or
   frame-by-frame .scr diff around a flap cycle).

## Work program (next session)

0. [DONE 2026-06-12] Finding 1's actual fix: slot-paint order in both
   compose paths; gate `test-enemy-flyover-redraw` wired in.

1. [DONE 2026-06-12] LAAD2 ported literally as `handling_blast_obj` (+$12
   cadence, +$13 nibble wrap) and wired into bird/ufo (replacing the
   `misc_12++ / &3` approximation) and blast (handler forces +$13=$90,
   frees the slot at sprite_num==9; the four kill sites now seed
   +$12=$50 / sprite_num=0 like kill_enemy $A4C4). `spr_ufo_frames`
   extended to the 10-entry anim_ufo ping-pong (1,2,3,4,5,6,5,4,3,2),
   indexed `% 10`; the UFO now animates at its true 3-frame period.
   The bird's $F0/$70 walk is IDENTICAL to the old approximation
   (verified: test-enemy-anim's pinned f8..f24 walk passes unchanged),
   so no visual change for birds — the UFO cadence/tail-frames and the
   blast's exact $50 lead-in are the parity gains, validated against
   the disasm (no UFO-level oracle replay exists; L3 is a bird round).
   Gates green: enemy anim/descend/steer, flyover-redraw,
   ball-object/brick-residue A/B, make test 7/7, frame-step floor,
   laffc-ball byte-exact.

   Original item text (kept for context): Port LAAD2 literally (`step_obj_anim`, the name it was planned under; it shipped as `handling_blast_obj`: +$12 cadence, +$13 nibble
   wrap) for bird/ufo (+blast: seeds $50/$90, free at frame 9); replace
   the port's `misc_12++ / &3` approximation (right 4-frame period for
   the bird, but LAAD2's reload `((res<<2)&$C0)|res` gives the UFO a
   3-frame period from its $60 seed — different cadence); extend
   `spr_ufo_frames` to the 10-entry anim_ufo order; index tables by
   sprite_num directly. NOTE the anim is already frame-deterministic —
   this item is ORIGINAL-PARITY work (cadence + table), not an A/B fix.
   Validate sprite_num against the original via the f24 oracle harness
   (both sides showed 4 at f24 with the current code, so differences
   surface at other frames/cadences — probe a few frames of the walk on
   both sides). Check `test-enemy-anim` (it pins the current invented
   walk and will need
   re-pinning to LAAD2 semantics).
2. Decode the rings (finding 4) and reconcile the sprite-data encoding
   (finding 3) — candidate fix: re-extract/patch the enemy sprites from
   the in-game snapshot (careful: other blob sprites carry runtime SMC
   like spr_magnet_circle_on height and spr_bonus_rocket_1 — patch
   selectively, not wholesale). Validate with the f24 oracle harness
   until port==original in the bird ROI.


## RESOLVED (2026-06-12, third pass): findings 3+4 dissolve — full decode

Both remaining findings turned out to be artifacts of mis-modelling,
plus two real bytes. The complete story:

**The encoding (finding 3).** The game's boot pass `gfx_inverse`
(preparation.asm) walks the sprite-pointer table chain from gfx_bat and
XORs every sprite's pix bytes with its mask bytes IN PLACE — that's the
whole `live_pix = blob_pix XOR mask` relation, uniform across all 49
sprites (verified byte-exhaustively, masks untouched). The chain's
duplicate entries all occur an ODD number of times (bat_gun_x ×3,
bird_1..3 ×3 via anim_bird's 11-entry walked list; the unlabeled plain
ufo/blast lists before the $0000 stop exist precisely so each enemy
sprite nets exactly one XOR despite the duped anim tables living AFTER
the terminator). The port's actual blit `(~m&d)|(m&p)` on TAPE data is
bit-identical to the original's `(m|s)^pix` on LIVE data for every
mask=1 bit, and NO shipped sprite has pix bits outside its mask (single
exception below), so mask=0 preserve-semantics is equivalent too.
There was never a data bug — the earlier "renders a DIFFERENT image"
ASCII comparison used the FICTIONAL formula from the stale comment
above blit_masked_to_scr_buff_ptr (now rewritten to match the code).

**The two real bytes.** bird_4's header claims 15 rows but the layout
allots 14: gfx_inverse overruns 3 pairs into spr_bird_5, (a) XORing its
header height $12^$03 = $11 — the original draws bird_5 with 17 rows,
not 18 — and (b) double-XORing bird_5's first data pair back to its
tape value, which the original then renders as INK (live pix 00 under
mask $30 -> (1|s)^0 = 1) where the port rendered PAPER. Both are now
reproduced at the asset level (the sprites.bin Makefile rule patches
hdr h -> $11 and pair-0 pix -> $30). bird_4's own garbage 15th row
(bird_5's header bytes read as a row) renders equivalently in the port
by luck of the same XOR algebra, except ONE bit (pix bit outside mask —
the only such bit in the game) that the original XORs into the
background; accepted, ~1 px under a garbage row.

**The "rings" (finding 4).** Not bird-related at all: clustering each
side's f24 capture against the static GT shows the original's extra
blobs at rows 12..28 in three groups (x 16..62 / 103..129 / 175..238)
= the 1UP/HI/2UP HUD areas — the live original had accumulated score
(the seeded ball broke bricks) and HUD blink state vs the alien-free,
score-zero GT and the port's BATTYALL scoreless HUD. The bird blob
itself matches between port and original. The 38 px f24 "delta" was
HUD, inherent to comparing a scoreless test HUD against the live
original — not a render gap.

**Net state.** Bird/UFO render parity is now fully decoded and closed:
anim driver (LAAD2), draw order ($9AD0 slots), sprite encoding
(gfx_inverse + the bird_5 overrun bytes). No open items remain in this
note. Gates: enemy anim/flyover/dirty A/B, make test 7/7, frame-step
floor, laffc-ball byte-exact — all green with the patched asset.


## RESOLVED (known-bugs #7, 2026-06-17): the enemy must NOT recolour its cells

The last enemy-render divergence. The port painted the flying enemy's
whole bounding box to `bg_attr` via `blit_sprite_attrs_to_buff(enemy...,
bg_attr)` (one call in each of the two compose paths). The original does
**not** do this: moving objects are drawn by `print_obj_to_buff` ($B82C),
which blits sprite PIXELS only and **never** calls `print_sprite_attrib`
($B656). `print_sprite_attrib` is invoked exactly 4 times in the whole
game, all inside `game_screen_draw_to_buffer` (the static texture + border
paint) — never for a moving object.

So in the original the bird/UFO keeps each cell's *underlying* attr: the
playfield `bg_attr` over open texture, and the **brick's** attr over
bricks — i.e. ZX colour-clash (the bird over a red brick shows red, not
the background). The port's `bg_attr` recolour repainted every brick cell
the bird flew over to the playfield background (on L1, `bg_attr`=0x46 =
bright-yellow ink / black paper, so the bird's bricks went yellow/black —
the user's "black background" report).

**Oracle proof.** `capture_frame_timeline_original.py
--setup-from-replay replays/l3-enemy-flyover.json --probe-ball 0x9B96`
(build/orig_flyover) shows the bird's cell attrs are **byte-identical to
the static L3 GT** (`build/level_gt/level_03.scr`) at every captured frame
— brick cells (0x05/0x57) and bg cells (0x45) all unchanged under the
flying bird. The original never touches them.

**Fix.** Dropped both `blit_sprite_attrs_to_buff(enemy..., bg_attr)` calls
(full + simple compose paths) and removed the now-dead
`blit_sprite_attrs_to_buff` helper; the enemy now blits pixels only, like
every other moving object (ball/bullet/bonus/rocket already did — they
ignore their `bg` arg, "no per-cell attr override"). This also dissolves
the bug-#2 stale-clash-attr class entirely (there is no recolour left to
leave stale). **Gate:** `make test-enemy-attr-parity` — a pure
port-internal invariant (no ZEsarUX): under a flying enemy seeded over L3
brick rows, `attr_buff` must equal `bg_attr_buff` (the static-background
snapshot) across the sprite's footprint. Regression-checked green:
test-enemy-brick-residue, test-enemy-flyover-redraw, test-enemy-descend,
test-enemy-anim.
