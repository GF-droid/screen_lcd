#include "proto.h"
#include "ws_client.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int proto_build(uint8_t type, const uint8_t *payload, int len, uint8_t *out) {
    out[0] = type;
    if (len > 0 && payload) memcpy(out + 1, payload, len);
    return 1 + len;
}

int proto_send_json(void *ws_conn, uint8_t type, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0 || n >= (int)sizeof(buf)) return -1;
    uint8_t frame[1025];
    int flen = proto_build(type, (const uint8_t *)buf, n, frame);
    return ws_send_frame((ws_conn_t *)ws_conn, 0x2, frame, flen);
}

/* ---------- JSON 取值 (strstr 定位 + 手工解析, 格式固定够用) ---------- */

/* 在 json 中找 "key": 之后的值起点 */
static const uint8_t *find_value(const uint8_t *json, int len, const char *key) {
    char pat[96];
    int kn = snprintf(pat, sizeof(pat), "\"%s\":", key);
    if (kn <= 0 || kn >= (int)sizeof(pat)) return NULL;
    for (int i = 0; i + kn <= len; i++) {
        if (memcmp(json + i, pat, kn) == 0) {
            const uint8_t *p = json + i + kn;
            while (p < json + len && (*p == ' ' || *p == '\t')) p++;
            return p;
        }
    }
    return NULL;
}

int json_get_str(const uint8_t *json, int len, const char *key,
                 char *buf, int cap) {
    const uint8_t *p = find_value(json, len, key);
    if (!p || p >= json + len || *p != '"') return -1;
    p++;
    int n = 0;
    while (p < json + len && n < cap - 1) {
        if (*p == '"') { buf[n] = '\0'; return 0; }
        if (*p == '\\' && p + 1 < json + len) p++; /* 简单转义跳过 */
        buf[n++] = *p++;
    }
    buf[n] = '\0';
    return 0;
}

int json_get_int(const uint8_t *json, int len, const char *key, int *out) {
    const uint8_t *p = find_value(json, len, key);
    if (!p || p >= json + len) return -1;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    long v = 0;
    int digits = 0;
    while (p < json + len && *p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
        digits++;
    }
    if (!digits) return -1;
    *out = (int)(neg ? -v : v);
    return 0;
}
