# Plan — gameplay recreation

The menu phases are done (TITLE → MENU → HISCORE, all checkpoints
pixel-identical). This was the plan for porting the actual game.

Original [`plan.md`](plan.md) covered the menu phase and is now
historical reference.

## What's not yet implemented

- 2-player mode (`game_mode == 2`) — selected from menu but never
  wired into `run_level`. Per-player score/lives/bat/ball
  alt-turn logic absent.
- Real `handling_ball` direction-byte (~64-angle) motion — the
  port uses integer dx/dy with a 5-zone bat-deflection. Largely
  invisible to the player unless they're doing 1:1 timing
  comparisons.
- Frame ornament from the original's `spr_bord_*` sprite
  primitives — currently bundles a captured `frame_l1.bin`
  blit, which is per-level identical but won't track if we
  ever need to draw outside the L1 cycle palette mapping.

