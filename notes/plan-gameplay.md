# Plan — gameplay recreation

The menu phases are done (TITLE → MENU → HISCORE, all checkpoints
pixel-identical). This was the plan for porting the actual game.

Original [`plan.md`](plan.md) covered the menu phase and is now
historical reference.

## Status — 2026-05-16 (post-multi-ball / laser / L4-spark)

The phases below are now mostly **historical** — gameplay,
rendering, brick types, bonuses, and the L4-specific enemy are
implemented. The blitter port (Phase B–C work) became its own
discrete refactor and is documented in
[`blitter-port.md`](blitter-port.md).

What works today:

- Buffer pipeline: `scr_buff` + `attr_buff` mirror ZX VRAM; one
  `buff_to_vga` pass per frame. Every renderer (bg, bricks, bat,
  lives, ball, ball2, bomb, 400pts, alien/spark/bird/UFO, bonus,
  frame ornament, bullet) writes via the buffer.
- Brick types: 1-hit / multi-hit / undestructible per the original
  cell-byte encoding (bit 7 = no brick / destroyed, bit 5 =
  undestructible, bit 4 = "this hit destroys"). Multi-hit bricks
  dim after first hit.
- Bonuses (10 types wired): LIFE, SLOW, BIG_BAT, BIG_BALL,
  KILL_ALIENS, CATCH, ROCKET, SCORE_5K, LASER, MULTI_BALL.
- Active-effect HUD chips for SLOW / BIG_BAT / BIG_BALL /
  KILL_ALIENS / CATCH / LASER / MULTI_BALL.
- L4 spark enemy (`handling_spark_obj`) — 5-frame decaying bouncer.
- HUD: 1UP score + HI score in the top strip (char row 1, idx 6
  yellow); both skip rendering when 0 so the frame's baked digits
  show through; HI rolls forward mid-game.
- state4_level1 floor: **194 px** vs the captured ZX GT — entirely
  the intentional bat+ball overlay over the bat-free GT snapshot.

What's not yet implemented:

- `input_new_record_name` — high-score entry alphabet grid + name
  capture; today we just save the score.
- 2-player mode (`game_mode == 2`) — selected from menu but never
  wired into `run_level`. Per-player score/lives/bat/ball alt-turn
  logic absent.
- Real `handling_ball` direction-byte (~64-angle) motion — the
  port uses integer dx/dy with a 5-zone bat-deflection. Largely
  invisible to the player unless they're doing 1:1 timing
  comparisons.
- Animated rocket flight when ROCKET is caught — currently
  insta-destroys all destructible cells.

