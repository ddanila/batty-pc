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

1. **The two PORT paths agree**: port-dirty vs port-full at f24 = 0 px
   in the bird ROI. Combined with the f60/70/80/100 healing (previous
   session), the repro's "21 px" at f50 is NOT a dirty-path bug — it is
   the **anim-phase A/B artifact**: the port's bird/ufo anim step is
   gated on `(pit_frame_counter & 3) == 0`, and the PIT value at game
   frame N differs between a dirty boot and a (slower) full-flush boot,
   so the two boots draw different anim steps at the same game frame.

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

1. Port LAAD2 literally (`step_obj_anim`: +$12 cadence, +$13 nibble
   wrap) for bird/ufo (+blast: seeds $50/$90, free at frame 9); drop the
   global-phase counter; extend `spr_ufo_frames` to the 10-entry
   anim_ufo order; index tables by sprite_num directly. Re-run
   `repro_enemy_flyover_trail.py` — expected to go 0 px (finding 1);
   then rename to test_* and wire into parity-check-full. Check
   `test-enemy-anim` (it pins the current invented walk and will need
   re-pinning to LAAD2 semantics).
2. Decode the rings (finding 4) and reconcile the sprite-data encoding
   (finding 3) — candidate fix: re-extract/patch the enemy sprites from
   the in-game snapshot (careful: other blob sprites carry runtime SMC
   like spr_magnet_circle_on height and spr_bonus_rocket_1 — patch
   selectively, not wholesale). Validate with the f24 oracle harness
   until port==original in the bird ROI.
