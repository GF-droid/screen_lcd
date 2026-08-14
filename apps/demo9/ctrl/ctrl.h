#ifndef _CTRL_CTRL_H_
#define _CTRL_CTRL_H_

#include <stdint.h>

typedef struct demo9_ctx demo9_ctx_t;

/* 音量/亮度/闹钟控制状态 (agent 全局, 各线程读; int 对齐读写即原子) */
typedef struct {
    int vol;          /* 0-100 音量 (Soft Volume Master) */
    int bright;       /* 0-100 亮度 (backlight) */
    int alarm_h;      /* 0-23 闹钟时 */
    int alarm_m;      /* 0-59 闹钟分 */
    int alarm_on;     /* 0/1 闹钟开关 */
    int alarm_fired;  /* 1 = 正在响铃 (alarm 线程置位, BS 上报) */
} ctrl_state_t;

/* 启动时读当前值 (amixer/backlight/alarm.conf), 失败用回退值 */
void ctrl_init(demo9_ctx_t *ctx);
/* T_CTRL (0x0B) JSON 分发: vol / bright / alarm_h+alarm_m+alarm_on */
void ctrl_handle(demo9_ctx_t *ctx, const uint8_t *json, int len);
/* 音量/亮度: 0-100; 真机走 amixer/backlight, 模拟器打日志 */
int ctrl_vol_set(demo9_ctx_t *ctx, int pct);
int ctrl_bright_set(demo9_ctx_t *ctx, int pct);
/* 闹钟: 启动读持久化配置 / 设置并持久化 */
void ctrl_alarm_load(demo9_ctx_t *ctx);
void ctrl_alarm_set(demo9_ctx_t *ctx, int h, int m, int on);
/* 闹钟线程: 每秒比对, 到点响铃 (aplay), 置 fired 供 BS 上报 */
void *alarm_thread_main(void *arg);

#endif  // _CTRL_CTRL_H_
