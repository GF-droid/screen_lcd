#include "demo9.h"
#include "net/proto.h"
#include "ctrl/ctrl.h"
#include "status/board_status.h"
#include "osal/osal_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

static demo9_ctx_t g_ctx;

static uint64_t mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void on_signal(int s) {
    (void)s;
    g_ctx.running = 0;
}

/* 收帧分发 (ws_thread 的 ws_poll 回调上下文) */
static void on_frame(ws_conn_t *c, int opcode, const uint8_t *payload,
                     int len, void *ud) {
    demo9_ctx_t *ctx = ud;
    if (opcode != 0x2 || len < 1) return;
    ctx->last_recv = mono_ms();   /* 任意入站帧都刷新假死计时 */
    switch (payload[0]) {
    case T_CTRL:      /* 浏览器控制: 音量/亮度/闹钟 */
        ctrl_handle(ctx, payload + 1, len - 1);
        break;
    case T_HB:        /* 服务器回显心跳: 收到即证明链路活 */
        break;
    case T_ERR: {
        char msg[128] = "";
        json_get_str(payload + 1, len - 1, "msg", msg, sizeof(msg));
        fprintf(stderr, "[ws] 服务器错误: %s\n", msg);
        break;
    }
    default:
        break;
    }
}

/* ws 主线程: 连接/退避重连/收包分发/心跳/状态上报 */
static void *ws_thread_main(void *arg) {
    demo9_ctx_t *ctx = arg;
    int backoff_ms = 1000;
    uint64_t last_hb = 0, last_bs = 0;

    while (ctx->running) {
        if (!ctx->connected) {
            snprintf(ctx->status_line, sizeof(ctx->status_line),
                     "连接 %s ...", ctx->url);
            fprintf(stderr, "[ws] 连接 %s\n", ctx->url);
            ws_close(&ctx->ws);   /* 清掉旧 fd (发送失败等路径可能未关), 防泄漏 */
            if (ws_connect(&ctx->ws, ctx->url, 8000) == 0) {
                ctx->connected = 1;
                backoff_ms = 1000;
                ctx->last_recv = last_hb = last_bs = mono_ms();
                pthread_mutex_lock(&ctx->send_lock);
                proto_send_json(&ctx->ws, T_HELLO,
                                "{\"v\":1,\"role\":\"board\",\"name\":\"%s\"}",
                                ctx->name);
                pthread_mutex_unlock(&ctx->send_lock);
                fprintf(stderr, "[ws] 已连接, HELLO 已发\n");
            } else {
                snprintf(ctx->status_line, sizeof(ctx->status_line),
                         "连接失败, %dms 后重试", backoff_ms);
                fprintf(stderr, "[ws] 连接失败, %dms 后重试\n", backoff_ms);
                osal_thread_sleep(backoff_ms);
                backoff_ms = backoff_ms * 2 > RETRY_MAX_MS
                                 ? RETRY_MAX_MS : backoff_ms * 2;
                continue;
            }
        }

        /* 收包 (最多等 200ms) */
        int r = ws_poll(&ctx->ws, 200);
        if (r < 0) {
            fprintf(stderr, "[ws] 连接断开\n");
            ws_close(&ctx->ws);
            ctx->connected = 0;
            backoff_ms = 1000;
            continue;
        }

        uint64_t now = mono_ms();

        /* 心跳: 每 5s 发一帧 (服务器回显即证明链路双向通) */
        if (now - last_hb >= (uint64_t)HEARTBEAT_S * 1000) {
            last_hb = now;
            pthread_mutex_lock(&ctx->send_lock);
            proto_send_json(&ctx->ws, T_HB, "{\"t\":\"hb\",\"ts\":%llu}",
                            (unsigned long long)now);
            pthread_mutex_unlock(&ctx->send_lock);
        }

        /* 假死检测: 15s 无任何入站帧 → 强制重连 */
        if (now - ctx->last_recv >= HB_TIMEOUT_MS) {
            fprintf(stderr, "[ws] 心跳超时 (%dms 无数据), 强制重连\n",
                    HB_TIMEOUT_MS);
            ws_close(&ctx->ws);
            ctx->connected = 0;
            backoff_ms = 1000;
            continue;
        }

        /* 板端状态: 每 2s 上报 (含音量/亮度/闹钟/CPU, 前端曲线用) */
        if (now - last_bs >= (uint64_t)BS_INTERVAL_S * 1000) {
            last_bs = now;
            board_status_send(ctx);
        }
    }
    fprintf(stderr, "[ws] 线程退出\n");
    return NULL;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "用法: %s [ws://host:port/ws/board] [--token xxx] [--name xxx] [--fake]\n"
            "  --name    板名 (默认 t113-01; 同名旧连接会被服务器踢掉, 多实例须不同名)\n"
            "  --fake    x86 模拟模式 (音量/亮度/闹钟只打日志, 不操作真实设备)\n",
            argv0);
}

int main(int argc, char **argv) {
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.ws.fd = -1;
    g_ctx.ws.on_frame = on_frame;   /* ws_connect 不覆盖这两个字段, 提前注册 */
    g_ctx.ws.ud = &g_ctx;
    g_ctx.running = 1;
    pthread_mutex_init(&g_ctx.send_lock, NULL);
    snprintf(g_ctx.status_line, sizeof(g_ctx.status_line), "启动中");

    const char *url = DEFAULT_URL;
    const char *token = DEFAULT_TOKEN;
    const char *name = "t113-01";

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--fake")) g_ctx.fake_mode = 1;
        else if (!strcmp(a, "--token") && i + 1 < argc) token = argv[++i];
        else if (!strcmp(a, "--name") && i + 1 < argc) name = argv[++i];
        else if (!strcmp(a, "--url") && i + 1 < argc) url = argv[++i];
        else if (!strncmp(a, "ws://", 5)) url = a;
        else { usage(argv[0]); return 1; }
    }
    snprintf(g_ctx.name, sizeof(g_ctx.name), "%s", name);

    /* token + name 拼到 URL query (服务器按 name 登记/踢旧连接) */
    snprintf(g_ctx.url, sizeof(g_ctx.url), "%s%s%s=%s&name=%s", url,
             strchr(url, '?') ? "&" : "?", "token", token, name);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    ctrl_init(&g_ctx);   /* 启动读音量/亮度/闹钟当前值 */
    fprintf(stderr, "[demo9] url=%s fake=%d 音量=%d%% 亮度=%d%% 闹钟=%02d:%02d%s\n",
            g_ctx.url, g_ctx.fake_mode, g_ctx.ctrl.vol, g_ctx.ctrl.bright,
            g_ctx.ctrl.alarm_h, g_ctx.ctrl.alarm_m,
            g_ctx.ctrl.alarm_on ? "开" : "关");

    osal_thread_t th_ws, th_alarm;
    if (osal_thread_create(&th_ws, ws_thread_main, &g_ctx) != OSAL_SUCCESS ||
        osal_thread_create(&th_alarm, alarm_thread_main, &g_ctx) != OSAL_SUCCESS) {
        fprintf(stderr, "[demo9] 线程创建失败\n");
        return 1;
    }

    while (g_ctx.running) osal_thread_sleep(200);
    fprintf(stderr, "[demo9] 收到退出信号, 收尾...\n");

    /* 唤醒 ws_thread 阻塞的 select */
    if (g_ctx.ws.fd >= 0) close(g_ctx.ws.fd);
    osal_thread_join(&th_ws, NULL);
    osal_thread_join(&th_alarm, NULL);
    ws_close(&g_ctx.ws);
    pthread_mutex_destroy(&g_ctx.send_lock);
    fprintf(stderr, "[demo9] 已退出\n");
    return 0;
}
