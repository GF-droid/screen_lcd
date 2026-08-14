#ifndef _DEMO9_H_
#define _DEMO9_H_

#include <pthread.h>
#include "cfg.h"
#include "net/ws_client.h"
#include "ctrl/ctrl.h"

/* 全局运行上下文 (main.c 初始化, 各线程共享) */
struct demo9_ctx {
    ws_conn_t ws;               /* ws_thread 持有, 连接/收包都在该线程 */
    pthread_mutex_t send_lock;  /* 所有 ws_send_frame 先加锁 (HB/BS/响应并发发) */
    char url[512];              /* 实际连接地址 (含 token/name query) */
    char name[64];              /* 板名 (服务器按名登记, 同名的旧连接会被踢) */
    int running;                /* 1=运行, 0=SIGINT 后退出 */
    int fake_mode;              /* 1=x86 模拟模式 (音量/亮度/闹钟只打日志) */
    int connected;              /* ws 当前是否连接 */
    char status_line[128];      /* 最近连接错误/状态 (日志用) */
    uint64_t last_recv;         /* 最近一次入站帧时刻 (假死检测, 回调刷新) */
    ctrl_state_t ctrl;          /* 音量/亮度/闹钟状态 (ctrl 线程与 ws 线程共享) */
};

#endif  // _DEMO9_H_
