#ifndef _DEMO9_CFG_H_
#define _DEMO9_CFG_H_

/* demo9 远程控制 agent — 全局常量与配置 */

/* 消息类型 (与 server/server.py 与前端 app.js 保持一致) */
enum {
    T_HELLO   = 0x01,
    T_HB      = 0x04,
    T_STATUS  = 0x06,
    T_ERR     = 0x07,
    T_BS      = 0x09,
    T_CTRL    = 0x0B,   /* 浏览器→服→板: 音量/亮度/闹钟设置 */
};

#define DEFAULT_URL    "ws://127.0.0.1:9000/ws/board"
#define DEFAULT_TOKEN  "t113demo"

#define HEARTBEAT_S    5          /* 应用层心跳间隔 */
#define HB_TIMEOUT_MS  15000      /* 心跳超时 → 判假死重连 */
#define BS_INTERVAL_S  2          /* 板端状态上报间隔 (曲线需要 2s 粒度) */
#define RETRY_MAX_MS   30000

/* 全局运行时上下文 (demo9_ctx_t, 见 main.c) */
typedef struct demo9_ctx demo9_ctx_t;

#endif  // _DEMO9_CFG_H_
