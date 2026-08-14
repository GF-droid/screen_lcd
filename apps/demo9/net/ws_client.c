#include "ws_client.h"
#include "sha1.h"
#include "base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <time.h>

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define RX_CAP_INIT 4096
#define RX_CAP_MAX (512 * 1024)

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ---------- TCP 连接 (带超时) ---------- */
static int tcp_connect(const char *host, int port, int timeout_ms) {
    struct addrinfo hints, *res = NULL;
    char portstr[16];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof(portstr), "%d", port);

    int ret = getaddrinfo(host, portstr, &hints, &res);
    if (ret != 0) {
        fprintf(stderr, "[ws] getaddrinfo %s: %s\n", host, gai_strerror(ret));
        return -1;
    }
    int fd = -1;
    int flags = 0;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        /* 非阻塞 connect + select 超时 */
        flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        if (errno == EINPROGRESS) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(fd, &wfds);
            struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
            int sr = select(fd + 1, NULL, &wfds, NULL, &tv);
            if (sr > 0) {
                int soerr = 0;
                socklen_t sl = sizeof(soerr);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl);
                if (soerr == 0) break;
            }
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd >= 0) {
        fcntl(fd, F_SETFL, flags); /* 恢复阻塞 (便于整帧读) */
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    }
    return fd;
}

/* ---------- 握手 ---------- */
static int parse_url(const char *url, char *host, int host_cap,
                     char *path, int path_cap, int *port) {
    if (strncmp(url, "ws://", 5) != 0) {
        fprintf(stderr, "[ws] 仅支持 ws:// (TLS 留待后续)\n");
        return -1;
    }
    const char *p = url + 5;
    const char *slash = strchr(p, '/');
    if (!slash) slash = p + strlen(p);
    /* host[:port] */
    const char *colon = NULL;
    for (const char *q = p; q < slash; q++)
        if (*q == ':') colon = q;
    if (colon) {
        int len = (int)(colon - p);
        if (len >= host_cap) len = host_cap - 1;
        memcpy(host, p, len);
        host[len] = '\0';
        *port = atoi(colon + 1);
    } else {
        int len = (int)(slash - p);
        if (len >= host_cap) len = host_cap - 1;
        memcpy(host, p, len);
        host[len] = '\0';
        *port = 80;
    }
    if (*port <= 0) *port = 80;
    snprintf(path, path_cap, "%s", *slash ? slash : "/");
    return 0;
}

static int ws_handshake(int fd, const char *host, int port,
                        const char *path, const char *key) {
    char req[1024];
    int n = snprintf(req, sizeof(req),
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s:%d\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Key: %s\r\n"
                     "Sec-WebSocket-Version: 13\r\n"
                     "\r\n",
                     path, host, port, key);
    if (write(fd, req, n) != n) return -1;

    /* 读 HTTP 响应头 (最多 4KB) */
    char resp[4096];
    int rlen = 0;
    while (rlen < (int)sizeof(resp) - 1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv = {10, 0};
        if (select(fd + 1, &rfds, NULL, NULL, &tv) <= 0) return -1;
        int got = read(fd, resp + rlen, sizeof(resp) - 1 - rlen);
        if (got <= 0) return -1;
        rlen += got;
        resp[rlen] = '\0';
        if (strstr(resp, "\r\n\r\n")) break;
    }
    resp[rlen] = '\0';
    if (strncmp(resp, "HTTP/1.1 101", 12) != 0) {
        fprintf(stderr, "[ws] 握手失败: %.*s\n", 200, resp);
        return -1;
    }
    /* 校验 Sec-WebSocket-Accept */
    char *acc = strstr(resp, "Sec-WebSocket-Accept:");
    if (!acc) { fprintf(stderr, "[ws] 缺 Sec-WebSocket-Accept\n"); return -1; }
    acc += 22;
    while (*acc == ' ' || *acc == '\t' || *acc == '\r' || *acc == '\n') acc++;
    char expect[64];
    uint8_t digest[20];
    char keybuf[128];
    snprintf(keybuf, sizeof(keybuf), "%s%s", key, WS_GUID);
    sha1((const uint8_t *)keybuf, strlen(keybuf), digest);
    b64_encode(digest, 20, expect);
    int alen = strcspn(acc, "\r\n");
    if (alen != (int)strlen(expect) || strncmp(acc, expect, alen) != 0) {
        fprintf(stderr, "[ws] Sec-WebSocket-Accept 校验失败\n");
        return -1;
    }
    return 0;
}

/* ---------- 帧收发 ---------- */
static int ws_read_more(ws_conn_t *c, int wait_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(c->fd, &rfds);
    struct timeval tv = {wait_ms / 1000, (wait_ms % 1000) * 1000};
    int sr = select(c->fd + 1, &rfds, NULL, NULL, &tv);
    if (sr <= 0) return sr; /* 0=超时 */
    if (c->rx_len >= c->rx_cap) return 0;
    int got = read(c->fd, c->rx_buf + c->rx_len, c->rx_cap - c->rx_len);
    if (got <= 0) return -1;
    c->rx_len += got;
    return 1;
}

/* 从累积缓冲解析并分发完整帧; 返回 1=有完整帧, 0=需更多数据 */
static int ws_parse_one(ws_conn_t *c) {
    uint8_t *b = c->rx_buf;
    int len = c->rx_len;
    if (len < 2) return 0;

    int fin = b[0] & 0x80;
    int opcode = b[0] & 0x0f;
    int masked = b[1] & 0x80;
    uint64_t plen = b[1] & 0x7f;
    int hdr = 2;
    if (plen == 126) {
        if (len < 4) return 0;
        plen = ((uint64_t)b[2] << 8) | b[3];
        hdr = 4;
    } else if (plen == 127) {
        if (len < 10) return 0;
        plen = 0;
        for (int i = 0; i < 8; i++) plen = (plen << 8) | b[2 + i];
        hdr = 10;
    }
    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked) {
        if (len < hdr + 4) return 0;
        memcpy(mask, b + hdr, 4);
        hdr += 4;
    }
    if ((uint64_t)len - hdr < plen) return 0;
    if (plen > RX_CAP_MAX) { c->rx_len = 0; return 1; } /* 异常帧丢弃 */

    uint8_t *payload = b + hdr;
    if (masked)
        for (uint64_t i = 0; i < plen; i++) payload[i] ^= mask[i & 3];

    switch (opcode) {
    case 0x1: case 0x2:
        if (fin && c->frag_opcode < 0 && c->on_frame)
            c->on_frame(c, opcode, payload, (int)plen, c->ud);
        break;
    case 0x0: /* continuation */
        if (c->frag_opcode >= 0 && fin && c->on_frame)
            c->on_frame(c, c->frag_opcode, payload, (int)plen, c->ud);
        if (fin) c->frag_opcode = -1;
        break;
    case 0x8: /* close */
        c->connected = 0;
        return -1;
    case 0x9: { /* ping → 回 pong */
        uint8_t pong[128];
        if (plen > 125) plen = 125;
        pong[0] = 0x8a;
        pong[1] = (uint8_t)plen;
        memcpy(pong + 2, payload, plen);
        if (write(c->fd, pong, 2 + plen) != 2 + (int)plen) c->connected = 0;
        break;
    }
    case 0xA: break; /* pong 忽略 */
    default: break;
    }
    /* 消费掉这一帧 */
    memmove(b, b + hdr + plen, len - hdr - plen);
    c->rx_len = len - hdr - plen;
    return 1;
}

/* ---------- 公开接口 ---------- */
int ws_connect(ws_conn_t *c, const char *url, int timeout_ms) {
    char host[128], path[512];
    int port;
    if (parse_url(url, host, sizeof(host), path, sizeof(path), &port) < 0)
        return -1;

    int fd = tcp_connect(host, port, timeout_ms);
    if (fd < 0) {
        fprintf(stderr, "[ws] 连接 %s:%d 失败\n", host, port);
        return -1;
    }
    /* 生成 Sec-WebSocket-Key: base64(16 随机字节) */
    uint8_t rand16[16];
    FILE *urnd = fopen("/dev/urandom", "rb");
    if (urnd) {
        if (fread(rand16, 1, 16, urnd) != 16) {
            srand((unsigned)now_ms());
            for (int i = 0; i < 16; i++) rand16[i] = rand() & 0xff;
        }
        fclose(urnd);
    } else {
        srand((unsigned)now_ms());
        for (int i = 0; i < 16; i++) rand16[i] = rand() & 0xff;
    }
    char key[32];
    b64_encode(rand16, 16, key);

    if (ws_handshake(fd, host, port, path, key) < 0) {
        close(fd);
        return -1;
    }
    c->fd = fd;
    c->connected = 1;
    c->frag_opcode = -1;
    c->rx_len = 0;
    c->rx_cap = RX_CAP_INIT;
    c->rx_buf = malloc(c->rx_cap);
    return 0;
}

int ws_send_frame(ws_conn_t *c, int opcode, const uint8_t *payload, int len) {
    if (!c->connected) return -1;
    uint8_t hdr[14];
    int n = 0;
    hdr[n++] = (uint8_t)(0x80 | opcode);
    if (len < 126) {
        hdr[n++] = (uint8_t)(0x80 | len);
    } else if (len < 65536) {
        hdr[n++] = 0x80 | 126;
        hdr[n++] = (uint8_t)(len >> 8);
        hdr[n++] = (uint8_t)(len & 0xff);
    } else {
        hdr[n++] = 0x80 | 127;
        for (int i = 7; i >= 0; i--) hdr[n++] = (uint8_t)(((uint64_t)len >> (8 * i)) & 0xff);
    }
    /* 客户端必须掩码 */
    uint8_t mask[4];
    srand((unsigned)(now_ms() ^ (uintptr_t)c));
    for (int i = 0; i < 4; i++) mask[i] = rand() & 0xff;
    memcpy(hdr + n, mask, 4);
    n += 4;
    int need = n + len;
    uint8_t *frame = malloc(need);
    memcpy(frame, hdr, n);
    for (int i = 0; i < len; i++) frame[n + i] = payload[i] ^ mask[i & 3];
    int total = 0;
    while (total < need) {
        int w = write(c->fd, frame + total, need - total);
        if (w <= 0) { free(frame); c->connected = 0; return -1; }
        total += w;
    }
    free(frame);
    return 0;
}

int ws_poll(ws_conn_t *c, int wait_ms) {
    if (!c->connected) return -1;
    int r = ws_read_more(c, wait_ms);
    if (r < 0) { c->connected = 0; return -1; }
    int handled = 0;
    for (;;) {
        int pr = ws_parse_one(c);
        if (pr < 0) { c->connected = 0; return -1; }
        if (pr == 0) break;
        handled = 1;
    }
    return 0;
}

void ws_close(ws_conn_t *c) {
    if (c->fd >= 0) close(c->fd);
    c->fd = -1;
    c->connected = 0;
    free(c->rx_buf);
    c->rx_buf = NULL;
    c->rx_len = c->rx_cap = 0;
}
