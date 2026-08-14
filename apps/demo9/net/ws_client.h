#ifndef _NET_WS_CLIENT_H_
#define _NET_WS_CLIENT_H_

#include <stdint.h>
#include <stddef.h>

/* RFC6455 WebSocket 客户端 — 纯 POSIX socket 自实现 (零外部依赖, MVP 为 ws:// 明文)
 * 用法: ws_connect → (可选) on_frame 回调注册 → 循环 ws_poll → ws_close
 * 收帧在 ws_poll 调用线程回调 (非独立线程) */

typedef struct ws_conn ws_conn_t;

/* 完整消息回调: opcode 0x1 text / 0x2 binary; payload 指向内部缓冲, 回调内有效 */
typedef void (*ws_on_frame_fn)(ws_conn_t *c, int opcode, const uint8_t *payload,
                               int len, void *ud);

struct ws_conn {
    int fd;
    int connected;                  /* 1=已连接 */
    uint8_t *rx_buf;                /* 累积接收缓冲 */
    int rx_len, rx_cap;
    int frag_opcode;                /* 分片累积: -1=无分片 */
    ws_on_frame_fn on_frame;
    void *ud;
};

/* 解析并连接 ws://host[:port]/path[?query]; 超时毫秒; 成功返回 0 */
int ws_connect(ws_conn_t *c, const char *url, int timeout_ms);

/* 发送一帧 (客户端帧自动加掩码); 返回 0 成功 */
int ws_send_frame(ws_conn_t *c, int opcode, const uint8_t *payload, int len);

/* 阻塞等待最多 wait_ms 毫秒读一帧并分发; 返回 0 正常, -1 连接已断开 */
int ws_poll(ws_conn_t *c, int wait_ms);

void ws_close(ws_conn_t *c);

#endif  // _NET_WS_CLIENT_H_
