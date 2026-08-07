/* See assets.h. */

#include <stdio.h>

#include "assets.h"

namespace { const char HIGH_SCORE_FILE[] = "HISCORE.DAT"; }

bool asset_load(const char *path, u8 *dest, unsigned size) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    const size_t got = fread(dest, 1, size, f);
    fclose(f);
    return got == size;
}

bool asset_load_variable(const char *path, u8 *dest, unsigned capacity,
                         unsigned *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    const size_t got = fread(dest, 1, capacity, f);
    fclose(f);
    *out_size = unsigned(got);
    return got > 0;
}

bool asset_load_chunked(const char *path, u8 *dest, unsigned size,
                        u8 *scratch, unsigned scratch_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    for (unsigned done = 0; done < size; ) {
        unsigned piece = size - done;
        if (piece > scratch_size) piece = scratch_size;
        if (fread(scratch, 1, piece, f) != piece) { fclose(f); return false; }
        for (unsigned i = 0; i < piece; i++) dest[done + i] = scratch[i];
        done += piece;
    }
    fclose(f);
    return true;
}

void high_score_load(unsigned long *score, u8 *name) {
    u8 buf[7];
    FILE *f = fopen(HIGH_SCORE_FILE, "rb");
    if (!f) return;
    const size_t got = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    if (got >= 4) {
        *score =  (unsigned long)buf[0]
               | ((unsigned long)buf[1] <<  8)
               | ((unsigned long)buf[2] << 16)
               | ((unsigned long)buf[3] << 24);
    }
    if (got >= 7) {
        name[0] = buf[4];
        name[1] = buf[5];
        name[2] = buf[6];
    }
}

void high_score_save(unsigned long score, const u8 *name) {
    FILE *f = fopen(HIGH_SCORE_FILE, "wb");
    if (!f) return;
    const u8 buf[7] = {
        u8(score), u8(score >> 8), u8(score >> 16), u8(score >> 24),
        name[0], name[1], name[2]
    };
    fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
}
