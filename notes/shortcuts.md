# Shortcuts — technical debt to repay

Running list of places where we ship captured asset bytes instead
of porting the logic that produced them. Each shortcut should be
repaid before its area becomes dynamic.

## Unresolved

### `assets/frame_l1.bin` — frame ornament pixels

The per-level frame ornament (HUD strip + side strips) is shipped
as a single 24 KB blob captured from the GT — 4 cycles × (top
24 px + 2 side strips). `paint_frame_to_buff` blits it directly.

The original computes the ornament at runtime from
`spr_bord_horiz_*` and `spr_bord_left/right_*` sprite primitives
(see `original/disasm/batty.asm` around line 6940). Porting that
would drop the captured-asset dependency and make the frame fully
deterministic from sprite data.

Status: not blocking anything visible; the captured blob renders
pixel-identical against the GT for L1..L4 cycles. Repay when we
need per-level frame variations the captured blob doesn't cover.

### `assets/level_attrs.bin` — per-level brick / frame attrs

The 15 × 768 B attr bands (one per level) are extracted from the
GT captures and copied wholesale into `attr_buff` for char rows
3..16. The original computes brick attrs dynamically via
`briks_colors[]` and `brik_shadow_c` (both ported); the only
piece still asset-shipped is the frame-strip cols within the
brick band, and the pre-dimmed magenta shadow attrs at the
shadow row of each brick.

Status: partial repayment. `print_one_brik_buf_c` writes brick
body attrs at runtime; destroyed cells reset to bg_attr. The
non-brick cells (frame strips, shadow rows in between bricks)
still use the captured values.

## Resolved (kept for cross-reference)

All other historical shortcuts (brick compositor cache, shipped
brick bitmap, dynamic score wiring, bat/lives wiring, sprite
cache pre-shift table) have been paid back. See
`notes/blitter-port.md` for the current rendering architecture.
