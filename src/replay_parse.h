/* replay_parse — the BATTY_REPLAY_* value formats.
 *
 * The replay harness seeds scenarios through environment variables, and
 * every one of them is a string a gate wrote. Two formats carry all of
 * them: a comma-separated integer list ("x,y", "type,x,y", "col,row,val")
 * and a hex blob of raw object bytes.
 *
 * These are pure: they take the value, not the variable. main.cpp does
 * the getenv. That is what makes them testable on the host, which
 * matters because a parser that silently half-succeeds would seed a
 * state nobody intended and the gate would report a game bug.
 *
 * Both are ALL-OR-NOTHING by design. A malformed value leaves `out`
 * untouched and the caller applies nothing.
 */

#ifndef BATTY_REPLAY_PARSE_H
#define BATTY_REPLAY_PARSE_H

#include "types.h"

/* The most fields any BATTY_REPLAY_* value carries (bonus is
 * "type,x,y"; BATTY_FORCE_BRICK is "col,row,value"). */
#define REPLAY_MAX_INTS 4

/* `count` comma-separated integers, strtol-style bases (so 0x.. and
 * negatives work). Any missing field, missing comma, or unparsable
 * number rejects the whole value and leaves `out` untouched. Trailing
 * text after the last field is ignored, matching the hand-rolled
 * parsers this replaced. Returns false if `count` exceeds
 * REPLAY_MAX_INTS. */
bool replay_parse_ints(const char *spec, long *out, int count);

/* Exactly `n` bytes as 2n hex digits, upper or lower case, and nothing
 * after them — a trailing character rejects the value rather than being
 * skipped, so a truncated blob cannot half-seed an object. */
bool replay_parse_hex_bytes(const char *spec, u8 *out, int n);

/* BATTY_VISUAL_PROBE_FRAMES: ascending absolute frame indices, comma
 * separated, spaces tolerated. Writes at most `max` values into `out`
 * and returns how many it kept.
 *
 * Values not STRICTLY greater than the one before are dropped rather
 * than rejected. That is not leniency for its own sake — the port walks
 * the list by subtracting consecutive entries, so a repeated or
 * out-of-order value would give a zero or negative countdown and the
 * run would either stall or skip a checkpoint. Dropping them keeps
 * every delta positive. */
int replay_parse_frame_list(const char *spec, unsigned int *out, int max);

#endif /* BATTY_REPLAY_PARSE_H */
