#include "base64.h"

static const char TBL[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int b64_encode(const uint8_t *in, int in_len, char *out) {
    int i, o = 0;
    for (i = 0; i + 2 < in_len; i += 3) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
        out[o++] = TBL[(v >> 18) & 63];
        out[o++] = TBL[(v >> 12) & 63];
        out[o++] = TBL[(v >> 6) & 63];
        out[o++] = TBL[v & 63];
    }
    if (in_len - i == 1) {
        uint32_t v = (uint32_t)in[i] << 16;
        out[o++] = TBL[(v >> 18) & 63];
        out[o++] = TBL[(v >> 12) & 63];
        out[o++] = '=';
        out[o++] = '=';
    } else if (in_len - i == 2) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8);
        out[o++] = TBL[(v >> 18) & 63];
        out[o++] = TBL[(v >> 12) & 63];
        out[o++] = TBL[(v >> 6) & 63];
        out[o++] = '=';
    }
    out[o] = '\0';
    return o;
}
