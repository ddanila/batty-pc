/* assets — reading the game's data files off the floppy.
 *
 * Every asset is a headerless blob whose layout the reader already knows,
 * so loading is "give me exactly this many bytes or fail". Nothing here
 * interprets what it read; that belongs to whichever module owns the
 * format.
 *
 * A short or missing file is always a failure rather than a partial load,
 * because a half-read sprite blob renders as garbage that looks like a
 * blitter bug. */

#ifndef BATTY_ASSETS_H
#define BATTY_ASSETS_H

#include "types.h"

/* Exactly `size` bytes, or false. */
bool asset_load(const char *path, u8 *dest, unsigned size);

/* Up to `capacity` bytes; `*out_size` receives how many arrived. False if
 * the file is missing or empty. For assets whose length is data, not a
 * constant — the markup streams are the only ones. */
bool asset_load_variable(const char *path, u8 *dest, unsigned capacity,
                         unsigned *out_size);

/* Exactly `size` bytes, read through `scratch` a piece at a time. For
 * blobs large enough that a single fread is worth avoiding. */
bool asset_load_chunked(const char *path, u8 *dest, unsigned size,
                        u8 *scratch, unsigned scratch_size);

/* --- The high-score file ---------------------------------------------- */
/* HISCORE.DAT: a 32-bit little-endian score, optionally followed by three
 * name bytes. Files written before the name existed are 4 bytes long and
 * still load — `name` is left untouched in that case. */

/* Always succeeds: a missing or unreadable file leaves both outputs as the
 * caller set them, which is how the defaults survive a first run. */
void high_score_load(unsigned long *score, u8 *name);
/* The DOS floppy is read/write under QEMU's if=floppy, so a save survives
 * a reboot — until `make floppy` rebuilds the image. */
void high_score_save(unsigned long score, const u8 *name);

#endif /* BATTY_ASSETS_H */
