/* See replay_parse.h. */

#include "replay_parse.h"

#include <stdlib.h>

namespace {

int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

}  /* namespace */

bool replay_parse_ints(const char *spec, long *out, int count) {
    /* Parsed into a scratch buffer and copied out only on success —
     * assigning straight into `out` would leave the fields that did
     * parse behind when a later one fails, which is exactly the
     * half-seeded state this is supposed to prevent. */
    long got[REPLAY_MAX_INTS];
    char *end;
    int i;
    if (spec == NULL || count < 1 || count > REPLAY_MAX_INTS) return false;
    for (i = 0; i < count; i++) {
        const char *start = (i == 0) ? spec : spec + 1;   /* step past ',' */
        got[i] = strtol(start, &end, 0);
        if (end == start) return false;
        if (i + 1 < count && *end != ',') return false;
        spec = end;
    }
    for (i = 0; i < count; i++) out[i] = got[i];
    return true;
}

bool replay_parse_hex_bytes(const char *spec, u8 *out, int n) {
    int i;
    if (spec == NULL) return false;
    for (i = 0; i < n; i++) {
        const int hi = hex_nibble(spec[i * 2]);
        const int lo = hex_nibble(spec[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (u8)((hi << 4) | lo);
    }
    return spec[n * 2] == '\0';
}
