#include "sha1.h"
#include <string.h>

#define ROL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

void sha1(const uint8_t *data, size_t len, uint8_t out[20]) {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu,
                     0x10325476u, 0xC3D2E1F0u};
    uint8_t block[64];
    size_t i, nblocks;
    uint64_t bitlen = (uint64_t)len * 8;

    nblocks = len / 64;
    for (i = 0; i < nblocks; i++) {
        const uint8_t *b = data + i * 64;
        uint32_t w[80];
        uint32_t a, b_, c, d, e, f, k, t;
        for (int j = 0; j < 16; j++) w[j] = be32(b + j * 4);
        for (int j = 16; j < 80; j++)
            w[j] = ROL32(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
        a = h[0]; b_ = h[1]; c = h[2]; d = h[3]; e = h[4];
        for (int j = 0; j < 80; j++) {
            if (j < 20)      { f = (b_ & c) | ((~b_) & d); k = 0x5A827999u; }
            else if (j < 40) { f = b_ ^ c ^ d;             k = 0x6ED9EBA1u; }
            else if (j < 60) { f = (b_ & c) | (b_ & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b_ ^ c ^ d;             k = 0xCA62C1D6u; }
            t = ROL32(a, 5) + f + e + k + w[j];
            e = d; d = c; c = ROL32(b_, 30); b_ = a; a = t;
        }
        h[0] += a; h[1] += b_; h[2] += c; h[3] += d; h[4] += e;
    }

    /* 末块 + 填充 */
    size_t rem = len % 64;
    memset(block, 0, sizeof(block));
    memcpy(block, data + nblocks * 64, rem);
    block[rem] = 0x80;
    if (rem >= 56) { /* 放不下 bitlen, 补满一块再新开一块 */
        uint32_t w[80];
        uint32_t a, b_, c, d, e, f, k, t;
        for (int j = 0; j < 16; j++) w[j] = be32(block + j * 4);
        for (int j = 16; j < 80; j++)
            w[j] = ROL32(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
        a = h[0]; b_ = h[1]; c = h[2]; d = h[3]; e = h[4];
        for (int j = 0; j < 80; j++) {
            if (j < 20)      { f = (b_ & c) | ((~b_) & d); k = 0x5A827999u; }
            else if (j < 40) { f = b_ ^ c ^ d;             k = 0x6ED9EBA1u; }
            else if (j < 60) { f = (b_ & c) | (b_ & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b_ ^ c ^ d;             k = 0xCA62C1D6u; }
            t = ROL32(a, 5) + f + e + k + w[j];
            e = d; d = c; c = ROL32(b_, 30); b_ = a; a = t;
        }
        h[0] += a; h[1] += b_; h[2] += c; h[3] += d; h[4] += e;
        memset(block, 0, sizeof(block));
    }
    for (int j = 0; j < 8; j++) block[56 + j] = (uint8_t)(bitlen >> (8 * (7 - j)));
    {
        uint32_t w[80];
        uint32_t a, b_, c, d, e, f, k, t;
        for (int j = 0; j < 16; j++) w[j] = be32(block + j * 4);
        for (int j = 16; j < 80; j++)
            w[j] = ROL32(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
        a = h[0]; b_ = h[1]; c = h[2]; d = h[3]; e = h[4];
        for (int j = 0; j < 80; j++) {
            if (j < 20)      { f = (b_ & c) | ((~b_) & d); k = 0x5A827999u; }
            else if (j < 40) { f = b_ ^ c ^ d;             k = 0x6ED9EBA1u; }
            else if (j < 60) { f = (b_ & c) | (b_ & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b_ ^ c ^ d;             k = 0xCA62C1D6u; }
            t = ROL32(a, 5) + f + e + k + w[j];
            e = d; d = c; c = ROL32(b_, 30); b_ = a; a = t;
        }
        h[0] += a; h[1] += b_; h[2] += c; h[3] += d; h[4] += e;
    }
    for (int i = 0; i < 5; i++) put_be32(out + i * 4, h[i]);
}
