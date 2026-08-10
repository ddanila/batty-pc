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
        /* Check the HIGH nibble before reading the low one. Reading both
         * first walks one byte past the terminator on any spec that ends
         * mid-pair — including the empty string, where `spec[1]` is off
         * the end of the literal. AddressSanitizer caught it on the
         * first run of `make test-asan`; the values come from BATTY_*
         * environment variables, so a short one reaches it. */
        const int hi = hex_nibble(spec[i * 2]);
        if (hi < 0) return false;
        const int lo = hex_nibble(spec[i * 2 + 1]);
        if (lo < 0) return false;
        out[i] = (u8)((hi << 4) | lo);
    }
    return spec[n * 2] == '\0';
}

int replay_parse_frame_list(const char *spec, unsigned int *out, int max) {
    unsigned int prev = 0;
    int count = 0;
    if (spec == NULL) return 0;
    while (*spec && count < max) {
        unsigned int v;
        while (*spec == ',' || *spec == ' ') spec++;
        if (*spec == '\0') break;
        v = (unsigned int)atoi(spec);
        if (v > prev) {
            out[count++] = v;
            prev = v;
        }
        while (*spec && *spec != ',') spec++;
    }
    return count;
}
