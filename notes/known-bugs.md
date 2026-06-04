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

The frame-step gate (`make capture-timeline-both`) reaches a **0 px
aligned start** (RGB space) comparing the port against the *snapshot*
original via the same `l3-brick-flash` setup, so the brick/magnet render
is NOT regressed. That points the `replay-l3-entry` failure at its
**live-tape-boot original's capture state/timing**: the probe RNG
divergence (`A187` vs `8E49`) suggests the live original advanced its
RNG differently than the snapshot path before the breakpoint capture.
Next: compare the `replay-l3-entry` original capture against the
snapshot-original timeline at frame 0 to localize whether it is an
RNG-pin gap in the live setup or a genuine capture-phase offset.
Discovered/triaged while building the frame-step parity gate
(iter 2026-06-04).
