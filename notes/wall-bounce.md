# Wall bounce — the reflect masks

Side walls use mask `$1F`, the top wall uses `$3F`. Getting them swapped
pinned balls against the left and right borders for a long time.

## The original

`bounce_wall` ($AC75) calls `change_direction` ($ACEE) with explicit masks:

    bounce_wall:
      LD B,$3F ; check_top_margin   -> change_direction(dir, $3F)   ; negate dy
      LD B,$1F ; check_left_margin  -> change_direction(dir, $1F)   ; negate dx
               ; check_right_margin -> change_direction(dir, $1F)   ; negate dx

    change_direction:  A = ((dir XOR B) + 1) AND $3F

That is the same `change_direction` `LAFFC` uses for brick faces, with the
same meaning of each mask — so the brick gate already validates the
arithmetic (`notes/laffc-decode.md`).

Note `bounce_wall` is the ball's and the sparks'. The ENEMY gets
`check_margins`, which clamps and does not reflect (`notes/enemy-movement.md`).

## The bug

`reflect_obj_dir` had the masks swapped AND off by one:

```c
if (flip_x) dir = (0x3F - dir) & 0x3F;   /* WRONG for a side wall */
if (flip_y) dir = (0x1F - dir) & 0x3F;   /* WRONG for the top wall */
```

The direction is a 6-bit angle (0=E, 16=S, 32=W, 48=N, clockwise, +y down).
`0x3F - dir` is the VERTICAL mask minus one, applied to the SIDE wall — so
hitting a side wall negated **dy** and left **dx** pointing into the wall:

```
ball dir 0x20 = (-255, 0), pure left, hits the left wall
  buggy: 0x3F - 0x20 = 0x1F = (-253, +24)  -> still moving LEFT
  next frame: clamped to x=8 again, 0x3F-0x1F=0x20 -> still LEFT
  => oscillates 0x1F <-> 0x20, pinned at x=8, dy juggling. STUCK.
```

The fix mirrors `change_direction`:

```c
if (flip_x) dir = ((dir ^ 0x1F) + 1) & 0x3F;   /* L/R wall: negate dx */
if (flip_y) dir = ((dir ^ 0x3F) + 1) & 0x3F;   /* top wall: negate dy */
```

Verified over all 64 directions: `flip_x` negates dx and preserves dy 64/64,
`flip_y` the converse. Every wall reflect — primary and secondaries — goes
through this one function.

## Why nothing caught it

It was long-standing and user-reported. The byte-exact L3 ball gate is
brick-collision-dominated and its trajectory never reaches a bare side wall,
and `make test` only checks static screens. **No gate had ever reached the
code.**

`make test-wall-bounce` now seeds a ball in the open band (y=0x90, below the
bricks and above the bat) aimed at a wall at speed 6, runs 8 frames, and
asserts it bounced clear — left `x >= 24`, right `x <= 220`. Validated to
PASS on the fix and FAIL on the reverted formula, which is the only way to
know a new gate is not a decoration.

**A sibling gate had been tuned to the bug.**
`test-ball-left-wall-escape` predates the fix and expected the wall-hit ball
in quadrant $00/$10 (downward) — which only "worked" because the wrong
vertical flip sent the ball DOWN into the bat seeded below it, and the BAT
deflection produced the escape the test observed. With the correct `$1F`
reflect the up-left $2C ball leaves the wall up-RIGHT as $34 and sits at
exactly x=$0F y=$8A after 12 frames; it asserts those values now. Caught on
the first full sweep after the fix.
