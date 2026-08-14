#include "ctrl.h"
#include "../demo9.h"
#include "osal/osal_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* 持久化: 板上 /data/app/alarm.conf, 模拟器用 cwd */
static const char *alarm_conf_path(void) {
#ifdef SIMULATOR_LINUX
    return "./alarm.conf";
#else
    return "/data/app/alarm.conf";
#endif
}

/* 启动读持久化闹钟 (格式 "h:m:on" 一行) */
void ctrl_alarm_load(demo9_ctx_t *ctx) {
    FILE *f = fopen(alarm_conf_path(), "r");
    if (f) {
        int h = -1, m = -1, on = 0;
        if (fscanf(f, "%d:%d:%d", &h, &m, &on) == 3 &&
            h >= 0 && h <= 23 && m >= 0 && m <= 59) {
            ctx->ctrl.alarm_h = h;
            ctx->ctrl.alarm_m = m;
            ctx->ctrl.alarm_on = on ? 1 : 0;
        }
        fclose(f);
    }
}

static void alarm_save(demo9_ctx_t *ctx) {
    FILE *f = fopen(alarm_conf_path(), "w");
    if (f) {
        fprintf(f, "%d:%d:%d\n", ctx->ctrl.alarm_h, ctx->ctrl.alarm_m,
                ctx->ctrl.alarm_on);
        fclose(f);
    }
}

/* 设置闹钟并持久化; 重置响铃状态 */
void ctrl_alarm_set(demo9_ctx_t *ctx, int h, int m, int on) {
    if (h < 0 || h > 23 || m < 0 || m > 59) return;
    ctx->ctrl.alarm_h = h;
    ctx->ctrl.alarm_m = m;
    ctx->ctrl.alarm_on = on ? 1 : 0;
    ctx->ctrl.alarm_fired = 0;
    alarm_save(ctx);
    fprintf(stderr, "[alarm] 设置 %02d:%02d %s\n", h, m,
            ctx->ctrl.alarm_on ? "开" : "关");
}

/* 响铃: 板上 aplay 后台播放 (与 demo8 widget_alarm.c 一致); 模拟器只打日志 */
static void alarm_play(demo9_ctx_t *ctx) {
    (void)ctx;
#ifdef SIMULATOR_LINUX
    fprintf(stderr, "[alarm] 响铃!\n");
#else
    system("killall aplay 2>/dev/null; aplay /data/res/music/audio_warn.wav &");
#endif
}

/* 闹钟线程: 每秒比对当前时:分 (edge 触发, 防每秒重复响) */
void *alarm_thread_main(void *arg) {
    demo9_ctx_t *ctx = arg;
    while (ctx->running) {
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        if (ctx->ctrl.alarm_on &&
            tm_now.tm_hour == ctx->ctrl.alarm_h &&
            tm_now.tm_min == ctx->ctrl.alarm_m) {
            if (!ctx->ctrl.alarm_fired) {
                ctx->ctrl.alarm_fired = 1;
                alarm_play(ctx);
                fprintf(stderr, "[alarm] 到点响铃 (%02d:%02d)\n",
                        ctx->ctrl.alarm_h, ctx->ctrl.alarm_m);
            }
        } else if (ctx->ctrl.alarm_fired) {
            ctx->ctrl.alarm_fired = 0;   /* 分钟错开, 允许下周期再响 */
        }
        osal_thread_sleep(1000);
    }
    fprintf(stderr, "[alarm] 线程退出\n");
    return NULL;
}
