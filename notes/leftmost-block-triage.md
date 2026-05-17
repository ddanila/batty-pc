# "Can't hit the leftmost block" — what I've ruled out

User report (iter 7-8):

> Laser bullet has the same attribute issue, also it's impossible to
> hit the leftmost block (have not had a chance to check the rightmost
> — please check how it's in the original game with that).
>
> same with the ball --- it does not hit the leftmost block

The attribute issue is fixed (see iters 6-7, `037c638`). The "can't
hit leftmost block" symptom for both ball and bullet I couldn't
reproduce from code reading. This note captures what I've ruled out
so an interactive repro can skip the dead ends.

## What "col 0" actually is

`LVL_COLS = 15`, so the level grid is cols 0..14. Col 0 occupies
x=8..23 in playfield coords. The **frame side strip** (left ornament)
also paints at x=0..23 (`FRAME_SIDE_W = 3` bytes). At y=57..62 the
strip bytes are `$9F $80 $00` — the strip's only ink in byte_x=1..2
is the single bit 7 at x=8, so cols 0 of bricks at byte_x=1..2 is
mostly visible through the strip's transparent regions. Per-level
brick layout (`assets/levels.bin`):

| Level | Leftmost VISIBLE brick (= bit4=0, bit7=0) |
|-------|-------------------------------------------|
| L1    | row 0 col 3 (`$07`)                       |
| L2    | row 0 col 2 (`$06`)                       |
| L3    | row 1 col 0 (`$06`) ← col 0!              |
| L4    | row 1 col 5 (`$06`)                       |
| L5    | row 0 col 0 (`$2E`, **undestructible**) ← col 0! |
| L6    | row 0 col 2 (`$07`)                       |
| L7    | row 0 col 1 (`$2B`, **undestructible**)   |
| L8    | row 0 col 1 (`$2B`, **undestructible**)   |
| L9    | row 0 col 2 (`$2C`, **undestructible**)   |
| L10   | row 2 col 3 (`$09`)                       |
| L11   | row 0 col 1 (`$07`)                       |
| L12   | row 0 col 2 (`$2B`, **undestructible**)   |
| L13   | row 5 col 1 (`$06`)                       |
| L14   | row 3 col 3 (`$07`)                       |
| L15   | row 0 col 0 (`$09`) ← col 0!              |

Hypothesis A: the user's "leftmost block" is at col 0 in L3 / L5 /
L15. For L5 the cell is `$2E` = undestructible by design (bit 5 = 1)
— ball/bullet bounces but can't destroy. That's correct behavior, not
a bug. L3 row 1 col 0 = `$06` should be destructible.

## What I verified is correct

- **Collision math** (`brick_collision` `src/main.c:2966`, bullet
  block `src/main.c:3302`): `col = (cx - 8) / 16`. For ball center
  `cx=12` → `col=0`. For bullet at `bullet_x=20` (= `BAT_X_MIN + 12`)
  → `col=0`. No off-by-one at the left edge.

- **Wall clamp** (`step_ball` `src/main.c:3490`): `BALL_X_MIN = 8`,
  ball can reach `x=8` (= cx=12 = col 0). Wall flip happens *before*
  `brick_collision` is called, and the brick check uses `next_x`
  (post-clamp). So a ball pinned at the wall still sees col 0.

- **Bullet fire X** (`src/main.c:4481`): `bullet_x = BAT_X + 12`,
  matches original `free_bullet_2` at `original/disasm/batty.asm:2210-2212`
  (`LD A,(IX+$02); ADD A,$0C`). With `BAT_X_MIN = 8`: min bullet_x=20,
  which falls in col 0 (x=8..23) per the collision math.

- **Cell-state semantics**: `$07` (= bit 4 clear) is multi-hit (needs
  2 hits per our convention, set bit 4 on first to mark damaged);
  `$13`/`$12`/`$14`/`$15` (= bit 4 set) destroy on first hit. `$2x`
  values with bit 5 set are undestructible. Matches the original's
  cell vocabulary (`notes/levels.md` got the bit-4 meaning wrong as
  "skip" — that's only `print_frame_metal_brik` at disasm line 4119,
  used for the metal-brick animation pass, not the main paint
  `print_line_briks` at 4174 which only checks bit 7).

- **Bat-collision range** (`eff_bat_left`/`eff_bat_right`,
  `src/main.c:2725-2726`): uses `BAT_BODY_W = 28`, matches the
  original's `object_bat_1+$0C = $1C` (= 28). Ball doesn't get
  "lost" past the visible right edge for any reason that wouldn't
  also apply to the original.

## What I noticed while looking (separate concern)

The bat sprite's MASK extent varies per row — rows 4-7 are wider
than the 28-px logical body. From my row-by-row scan:

```
row 0 (top):    24 px (x_sprite=2..25)
row 4:          29 px (x_sprite=0..28)
row 7 (widest): 32 px (x_sprite=0..31)
row 9 (bottom): 30 px
```

The 28-px logical body misses the bat sprite's MIDDLE-ROW outer
columns (x_sprite=28..31 → absolute x=144..147 with `BAT_X=116`).
But the original game has the same 28-px logical width, so this is
"matches the original", not a bug.

## What to try when reproducing

1. **Pin down which level** the user was on. Different levels have
   the leftmost block at different cols/rows; L5 / L9 / L7 / L8 / L12
   have undestructible metal at the leftmost positions — those are
   correct behavior, not bugs.
2. **Watch for a brick at col 0** being destroyed *invisibly*
   (cell bit 7 gets set but the brick continues to render somehow).
   Unlikely but possible if attr_buff retains the brick attr after
   the brick is gone.
3. **Check the bullet's actual path frame-by-frame**. The bullet
   collision is a point-vs-grid check on `bullet_x`. If the bullet
   sprite renders at one X but the collision uses a different X
   (off by the sprite's body-vs-sprite offset), the bullet would
   "miss" the column it appears to be in.
4. **Try BAT_X = 8** exactly and fire. Bullet at x=20 should hit
   col 0. If the brick at that (col, row) is `$2x` (undestructible),
   bullet bounces — that's not a "can't hit", it's "can't destroy".
