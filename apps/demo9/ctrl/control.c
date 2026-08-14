#include "ctrl.h"
#include "../demo9.h"
#include "../net/proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 音量写入: amixer Soft Volume Master (0-100, 与 demo8 widget_dev.c 一致) */
int ctrl_vol_set(demo9_ctx_t *ctx, int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
#ifdef SIMULATOR_LINUX
    fprintf(stderr, "[volume] %d%%\n", pct);
#else
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "amixer -c 0 sset 'Soft Volume Master' %d%% >/dev/null 2>&1", pct);
    system(cmd);
#endif
    ctx->ctrl.vol = pct;
    return 0;
}

/* 亮度写入: backlight 节点 0-255 (百分比×255/100, 与 demo8 一致) */
int ctrl_bright_set(demo9_ctx_t *ctx, int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
#ifdef SIMULATOR_LINUX
    fprintf(stderr, "[backlight] %d%%\n", pct);
#else
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "echo %d > /sys/class/backlight/backlight/brightness",
             pct * 255 / 100);
    system(cmd);
#endif
    ctx->ctrl.bright = pct;
    return 0;
}

/* 启动读当前值: 音量失败回退 80, 亮度失败回退 60 (与 demo8 一致) */
void ctrl_init(demo9_ctx_t *ctx) {
    ctx->ctrl.vol = 80;
    ctx->ctrl.bright = 60;
    ctrl_alarm_load(ctx);
#ifndef SIMULATOR_LINUX
    FILE *f = popen("amixer -c 0 sget 'Soft Volume Master' 2>/dev/null", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            char *p = strchr(line, '[');
            if (p && p[1] >= '0' && p[1] <= '9') {
                int v = atoi(p + 1);
                if (v >= 0 && v <= 100) ctx->ctrl.vol = v;
                break;
            }
        }
        pclose(f);
    }
    f = fopen("/sys/class/backlight/backlight/brightness", "r");
    if (f) {
        int raw = -1;
        if (fscanf(f, "%d", &raw) == 1 && raw >= 0 && raw <= 255)
            ctx->ctrl.bright = raw * 100 / 255;
        fclose(f);
    }
#else
    (void)ctx;
#endif
}

/* T_CTRL (0x0B) 分发: {"t":"ctrl","vol":50} / bright / alarm_h+alarm_m+alarm_on */
void ctrl_handle(demo9_ctx_t *ctx, const uint8_t *json, int len) {
    int v;
    if (json_get_int(json, len, "vol", &v) == 0) {
        ctrl_vol_set(ctx, v);
        return;
    }
    if (json_get_int(json, len, "bright", &v) == 0) {
        ctrl_bright_set(ctx, v);
        return;
    }
    if (json_get_int(json, len, "alarm_on", &v) == 0) {
        int h = -1, m = -1;
        if (json_get_int(json, len, "alarm_h", &h) == 0 &&
            json_get_int(json, len, "alarm_m", &m) == 0)
            ctrl_alarm_set(ctx, h, m, v);
        return;
    }
    fprintf(stderr, "[ctrl] 未识别字段: %.*s\n", len, json);
}
