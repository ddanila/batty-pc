# Bird / UFO rendering — anim driver, draw order, sprite encoding

The render half of the enemy. Motion and steering are in
`notes/enemy-movement.md`. All four items below are closed; the harness
that settled them is reusable.

## The oracle harness

`replays/l3-enemy-flyover.json` — the `l3-brick-flash` alignment plus a
poke of `object_enemy` ($9B96) with the fresh-spawn bird (x=168, y=1,
dir=$10, spd=1, +$12=$F0, +$13=$70), the same 22 bytes the port seeds via
`BATTY_REPLAY_ENEMY_OBJECT`. With `RANDOM=3793 SEED=962A COUNTER=2` both
sides are in positional lockstep through f24; the original re-picks its
target around f25-32 and diverges after, which is phase-gated.

    python3 scripts/capture_frame_timeline_original.py \
        --snapshot build/snapshots/20260513T202101Z.sna --frames 0,16,24 \
        --zesarux tools/zesarux/src/zesarux \
        --setup-from-replay replays/l3-enemy-flyover.json \
        --probe-ball 0x9B96 --out build/tl3_orig

Trick: passing `--probe-ball 0x9B95` (base - 1) makes the probe's "x"
field land on `sprite_num` (+$01) instead.

## The anim driver — `LAAD2` ($AAD2)

One stepper shared by the bird, UFO, blast, bat and bullet. `+$12` is a
cadence counter decremented by $40 per call; on underflow the sprite frame
`+$01` advances LINEARLY and wraps via `+$13`'s nibbles (high = last
frame, low = first frame), and `+$12` reloads as `((res << 2) & $C0) | res`.

| object | +$12 | +$13 | result |
|---|---|---|---|
| bird | `$F0` | `$70` | frames 0..7, 4 frames per step |
| UFO | `$60` | `$90` | frames 0..9, **3** frames per step |
| blast | `$50`, seeded by `kill_enemy` ($A4C4) | `$90`, forced by the handler | slot freed at frame 9 |

It is called unconditionally per frame once past the `y < 8` descend gate
— the UFO handler's `LD A,(counter_misc) / AND $00 / CALL Z` is an
always-taken quirk.

`anim_bird` is the 8-entry ping-pong `{1,2,3,4,3,2,1,5}` over
`spr_bird_1..5`; `anim_ufo` has **TEN** entries, `{1,2,3,4,5,6,5,4,3,2}`,
over six sprites. `spr_bird_frames[8]` and `spr_ufo_frames[10]` in
`src/main.cpp` match, indexed `& 7` and `% 10`, stepped by the literal
`LAAD2` port. The bird's `$F0`/`$70` walk is identical to the older
`misc_12++ / & 3` approximation it replaced — the UFO's cadence and tail
frames, and the blast's exact `$50` lead-in, are what the literal port
bought.

Because the counter is per-object, the animation is frame-deterministic.
Only the STEER gates on the global counter.

### `LAA02`'s facing mirror is DEAD CODE

It computes a remapped sprite number when the bird crosses a facing
hemisphere (`$0E - n`, or `(n ^ 7) + 7`) and never stores it: `LA9BC_5`
immediately does `LD A,(flag_2)`, overwriting A, with no `LD (IX+$01),A`
between. Raw bytes confirm both results are discarded — the
`LD A,(IX+$13) / LD (IX+$13),A` no-op nearby looks like patched-out
surgery. The ground truth agrees: sprite_num walks smoothly 4 -> 5 -> 6
through the f28 hemisphere flip with no remap jump. So the bird never
mirrors, and the ping-pong is the whole animation. `LAA02`'s only live
effect is its tail, the `flag_2`-gated brick-hit re-target.

## Draw order is object-slot order

The original paints in `$9AD0` slot order — balls 1-3, bullets, bats, the
shared bonus/bomb/pts400 slot ($9B80), ENEMY ($9B96), rocket ($9BAC) — so
later means on top, and bomb-under-enemy is correct.

A 21 px in-flight delta between the dirty and full compose paths turned
out to be exactly this: the simple path drew enemy-then-bomb, the full
path bomb-then-enemy, and the 21 px were one overlap rendered with
opposite stacking. Both paths follow slot order now, which also corrected
bonus-vs-enemy and extras-on-top. Gate: `test-enemy-flyover-redraw`.

**Localising it needed a third mode.** The f50 probes were byte-identical
on both boots — same position AND same sprite number — while the pixels
differed, so the anim phase was not the cause. `BATTY_FORCE_BALL_FULL_REDRAW`
(full COMPOSE, dirty FLUSH) matched the full-flush baseline exactly, which
put the bug in the simple path's COMPOSE rather than in its flush.

## Sprite encoding — `gfx_inverse` XORs pix with mask at boot

The boot pass walks the sprite-pointer table chain from `gfx_bat` and XORs
every sprite's pix bytes with its mask bytes IN PLACE, so
`live_pix = tape_pix XOR mask` uniformly across all 49 sprites (masks
untouched, verified byte-exhaustively). Every duplicate entry in the chain
occurs an ODD number of times — `bat_gun_x` three times, `bird_1..3` three
times via `anim_bird`'s 11-entry walked list — and the unlabelled plain
UFO/blast lists before the `$0000` stop exist precisely so each enemy
sprite nets exactly one XOR despite the duped anim tables living AFTER the
terminator.

The port's blit `(~m & d) | (m & p)` on TAPE data is bit-identical to the
original's `(m | s) ^ pix` on LIVE data for every mask=1 bit, and no
shipped sprite has pix bits outside its mask bar one, so the mask=0
preserve semantics agree too. There was never a data bug: the earlier
"renders a different image" comparison used a fictional formula from a
stale comment above the blit.

**Two real bytes.** `spr_bird_4`'s header claims 15 rows where the layout
allots 14, so `gfx_inverse` overruns 3 pairs into `spr_bird_5` and
(a) XORs its header height `$12 ^ $03 = $11`, so the original draws bird 5
with 17 rows rather than 18, and (b) double-XORs bird 5's first data pair
back to its tape value, which the original then renders as INK (live pix
00 under mask $30) where the port rendered PAPER. The `sprites.bin`
Makefile rule patches both. Bird 4's own garbage 15th row — bird 5's
header bytes read as a row — renders equivalently by the same XOR algebra
except for ONE bit, the only pix bit outside a mask in the game, which the
original XORs into the background. Accepted at ~1 px under a garbage row.

## Moving objects never recolour a cell (known-bugs #7)

`print_obj_to_buff` ($B82C) blits sprite PIXELS and never calls
`print_sprite_attrib` ($B656), which is invoked exactly four times in the
whole game, all inside `game_screen_draw_to_buffer` — never for a moving
object. So a flying bird keeps each cell's UNDERLYING attribute and shows
ZX colour clash: the BRICK's colour over bricks, `bg_attr` over open
texture.

The port had painted the enemy's whole bounding box to `bg_attr` (one call
in each compose path), which repainted every brick cell it flew over. On
L1 `bg_attr` is 0x46, bright yellow ink on black, which is the user's
"black background" report.

**Oracle proof:** the bird's cell attrs in `build/orig_flyover` are
byte-identical to the static L3 GT at every captured frame. Both calls are
gone and the helper with them; `test-enemy-attr-parity` holds it as a
port-internal invariant (`attr_buff == bg_attr_buff` across the sprite's
footprint), and `test-sprite-attr-parity` generalises it to every moving
sprite. `notes/video-engine.md` gates it a third way, exhaustively, on the
host.

## The "rings" were the HUD

At f24 both sides showed 7 px ring outlines flanking the bird, differently
placed, outside any bird frame's footprint. Clustering each side's capture
against the static GT put them at rows 12..28 in three groups matching the
1UP / HI / 2UP areas: the live original had accumulated score and blink
state, against an alien-free, score-zero GT and the port's `BATTYALL`
scoreless HUD. Not a render gap — inherent to comparing a scoreless test
HUD against a live original.
