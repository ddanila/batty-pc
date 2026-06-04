# Known bugs (user-reported, unfixed)

Concrete defects observed by the user that aren't yet caught by the
visual-regression test (= mid-game state we don't snapshot). Listed
here so the next iter has a target. When fixing, add a section to
`per-level-profile.md` or the relevant area doc; remove from here.

## `replay-l3-entry` parity gate regressed (0 → 1885 px)

`make replay-l3-entry`, documented in `notes/replay-harness.md` as
`l3_entry: 0/23040 px differ` / PASS, currently **FAILS at 1885/23040
px** (8.18%), diff bounds `(8,104,248,128)` — the magnet / lower-brick
band. State probes show the port RNG diverged (`random_number
port=A187` vs `original=8E49`) and `object_enemy` / `object_ball_1` /
`object_bat_1` differ.

Root-cause trail and the two candidate fixes are in
`notes/seeded-l3-entry-triage.md`: the entry-path `render_level_screen`
-> `render_magnets` -> `next_random` consumption desyncs the port RNG
from the original's pinned 8E49 (the original's metal-brick shimmer is
NOP'd via the `$BA6C` setup poke), and `play_brik_anim` leaves a
transient reveal-animation frame at the WAIT_KEY pause instead of the
settled brick field. Discovered while building the frame-step parity
gate (iter 2026-06-04); blocks that gate's aligned start.
