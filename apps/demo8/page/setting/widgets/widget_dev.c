#include "page_conf.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>

#include "widgets/widget_dev.h"
#include "widgets/widget_alarm.h"  /* 复用开关胶囊/分隔线样式 */
#include "widgets/widget_common.h"
#include "setting.h"              /* 顶栏音量/状态联动 */

lv_style_t style_bt_slider_main;  /* 轨道: 细长深蓝紫圆角条 */
lv_style_t style_bt_slider_ind;   /* 已滑部分: 亮蓝紫 */
lv_style_t style_bt_slider_knob;  /* 圆钮: 白底蓝边 */


/* ==================== 蓝牙 / 亮度 / 音量管理组件 (demo4 移植) ==================== */

static bool bt_slider_style_inited = false;

/* 亮度写入: 板上写背光 sysfs (0-255), 模拟器打印 */
static void brt_apply(int pct) {
#ifdef SIMULATOR_LINUX
    printf("[backlight] %d%%\n", pct);
    fflush(stdout);
#else
    char cmd[96];
    snprintf(cmd, sizeof(cmd), "echo %d > /sys/class/backlight/backlight/brightness", pct * 255 / 100);
    system(cmd);
#endif
}

/* 音量写入: 板上 amixer 调 Soft Volume Master (0-255), 模拟器打印 */
static void vol_apply(int pct) {
#ifdef SIMULATOR_LINUX
    printf("[volume] %d%%\n", pct);
    fflush(stdout);
#else
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "amixer -c 0 sset 'Soft Volume Master' %d%% >/dev/null 2>&1", pct);
    system(cmd);
#endif
}

/* 板上启动时读当前音量 (amixer 输出里的 [xx%]), 模拟器/失败时用 80% */
static int vol_read_init(void) {
#ifndef SIMULATOR_LINUX
    FILE* f = popen("amixer -c 0 sget 'Soft Volume Master' 2>/dev/null", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            const char* pct = strchr(line, '[');
            if (pct != NULL && pct[1] >= '0' && pct[1] <= '9') {
                int v = atoi(pct + 1);
                pclose(f);
                if (v >= 0 && v <= 100)
                    return v;
                return 80;
            }
        }
        pclose(f);
    }
#endif
    return 80;
}

/* 板上启动时读当前背光亮度, 模拟器/失败时用 60% */
static int brt_read_init(void) {
#ifndef SIMULATOR_LINUX
    FILE* f = fopen("/sys/class/backlight/backlight/brightness", "r");
    if (f) {
        int raw = -1;
        if (fscanf(f, "%d", &raw) == 1 && raw >= 0 && raw <= 255) {
            fclose(f);
            return raw * 100 / 255;
        }
        fclose(f);
    }
#endif
    return 60;
}

/* ---------- 蓝牙栏 (纯摆设: 开关只切 UI, 不接真实蓝牙) ---------- */
static lv_obj_t* g_bt_status_label = NULL;

/* 开关点击: CHECKED 态取反, 开关文案 + 状态大字同步 */
static void bt_switch_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    if (lv_obj_has_state(btn, LV_STATE_CHECKED))
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    else
        lv_obj_add_state(btn, LV_STATE_CHECKED);
    lv_obj_t* label = lv_event_get_user_data(e);
    bool on = lv_obj_has_state(btn, LV_STATE_CHECKED);
    lv_label_set_text(label, on ? "打开" : "关闭");
    lv_label_set_text(g_bt_status_label, on ? "连接" : "未连接");
}

/* 创建蓝牙栏: 标题 + 开关 + 状态大字 */
static lv_obj_t* bt_col_create(lv_obj_t* parent) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(col, false);

    /* 标题 */
    lv_obj_t* title = lv_label_create(col);
    lv_font_t* font_t = get_font(FONT_TYPE_CN, 14);
    if (font_t != NULL)
        lv_obj_set_style_text_font(title, font_t, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0x8A94C8), LV_STATE_DEFAULT);
    lv_label_set_text(title, "蓝牙");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    /* 开关胶囊: 中部 */
    lv_obj_t* sw = lv_btn_create(col);
    lv_obj_remove_style_all(sw);
    lv_obj_add_style(sw, &style_alarm_switch, LV_STATE_DEFAULT);
    lv_obj_add_style(sw, &style_alarm_switch_checked, LV_STATE_CHECKED);
    lv_obj_set_size(sw, 64, 24);
    lv_obj_align(sw, LV_ALIGN_CENTER, 0, -2);
    lv_obj_set_scrollable(sw, false);
    lv_obj_t* sw_label = lv_label_create(sw);
    lv_font_t* font_sw = get_font(FONT_TYPE_CN, 14);
    if (font_sw != NULL)
        lv_obj_set_style_text_font(sw_label, font_sw, LV_STATE_DEFAULT);
    lv_label_set_text(sw_label, "关闭");
    lv_obj_center(sw_label);
    lv_obj_add_event_cb(sw, bt_switch_cb, LV_EVENT_CLICKED, sw_label);

    /* 状态大字 */
    g_bt_status_label = lv_label_create(col);
    lv_font_t* font_big = get_font(FONT_TYPE_CN, 30);
    if (font_big != NULL)
        lv_obj_set_style_text_font(g_bt_status_label, font_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_bt_status_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(g_bt_status_label, "未连接");
    lv_obj_align(g_bt_status_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    return col;
}

/* ---------- 亮度/音量 (右栏, 上下两半) ---------- */
static lv_obj_t* g_brt_label = NULL;
static lv_obj_t* g_vol_label = NULL;

/* 亮度 slider: 拖动中大字实时跟随, 松手才写硬件 (避免拖动时频繁写 sysfs) */
static void brt_slider_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", val);
    lv_label_set_text(g_brt_label, buf);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED)
        brt_apply(val);
}

/* 音量 UI 同步: 右栏大字 + 顶栏 "音量:xx%" 联动 (同一状态只刷一遍) */
static void vol_set_ui(int val) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", val);
    lv_label_set_text(g_vol_label, buf);
    setting_top_vol_set(val); /* 顶栏音量联动 (setting.c 提供 setter, 不直接碰顶栏对象) */
}

/* 音量 slider: 同亮度, 松手写 amixer; 拖动中大字 + 顶栏实时跟随 */
static void vol_slider_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    vol_set_ui(val);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED)
        vol_apply(val);
}

/* 单条调节行: 标题(上) + 大字(左) + slider(右, 垂直居中) */
static void brtvol_row_create(lv_obj_t* parent, const char* title, int init_pct,
                              lv_obj_t** label_out, lv_event_cb_t cb) {
    /* 标题 */
    lv_obj_t* title_l = lv_label_create(parent);
    lv_font_t* font_t = get_font(FONT_TYPE_CN, 14);
    if (font_t != NULL)
        lv_obj_set_style_text_font(title_l, font_t, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title_l, lv_color_hex(0x8A94C8), LV_STATE_DEFAULT);
    lv_label_set_text(title_l, title);
    lv_obj_align(title_l, LV_ALIGN_TOP_MID, 0, 0);

    /* 大字百分比 */
    lv_obj_t* label = lv_label_create(parent);
    lv_font_t* font_v = get_font(FONT_TYPE_CN, 30);
    if (font_v != NULL)
        lv_obj_set_style_text_font(label, font_v, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", init_pct);
    lv_label_set_text(label, buf);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 6);
    *label_out = label;

    /* slider: 点轨道跳转 / 拖动连续调 */
    lv_obj_t* slider = lv_slider_create(parent);
    lv_obj_remove_style_all(slider);
    lv_obj_add_style(slider, &style_bt_slider_main, LV_STATE_DEFAULT);
    lv_obj_add_style(slider, &style_bt_slider_ind, LV_PART_INDICATOR);
    lv_obj_add_style(slider, &style_bt_slider_knob, LV_PART_KNOB);
    lv_obj_set_size(slider, 230, 24);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, init_pct, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, 0, 6);
    lv_obj_set_scrollable(slider, false);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_RELEASED, NULL);
}

/* 创建右栏: 上半亮度, 下半音量 */
static lv_obj_t* brtvol_col_create(lv_obj_t* parent) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(col, false);

    /* 上半: 亮度 (初始值读板上当前背光) */
    lv_obj_t* half1 = lv_obj_create(col);
    lv_obj_remove_style_all(half1);
    lv_obj_set_size(half1, LV_PCT(100), LV_PCT(50));
    lv_obj_align(half1, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_scrollable(half1, false);
    brtvol_row_create(half1, "亮度", brt_read_init(), &g_brt_label, brt_slider_cb);

    /* 下半: 音量 (初始读板上当前音量, 顶栏同步联动) */
    lv_obj_t* half2 = lv_obj_create(col);
    lv_obj_remove_style_all(half2);
    lv_obj_set_size(half2, LV_PCT(100), LV_PCT(50));
    lv_obj_align(half2, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_scrollable(half2, false);
    int vol0 = vol_read_init();
    brtvol_row_create(half2, "音量", vol0, &g_vol_label, vol_slider_cb);
    vol_set_ui(vol0); /* 创建后立刻刷一遍, 顶栏占位文本与实际音量对齐 */

    return col;
}

/* 每秒: 顶栏 WiFi 状态轮询 */
static void bt_check_timer_cb(lv_timer_t* t) {
    update_top_wifi_ui();
}

/* 蓝牙/亮度/音量组件: 卡片内左右分栏, 左=蓝牙摆设, 右=亮度/音量 */
lv_obj_t* bt_setting_create(lv_obj_t* parent) {
    alarm_manage_style_init(); /* 复用开关胶囊/分隔线样式 */
    bt_slider_style_init();

    /* 根容器: flex row 两栏 + 1px 竖分隔线 */
    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_layout(root, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(root, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(root, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    lv_obj_t* col_left = bt_col_create(root);
    lv_obj_set_flex_grow(col_left, 1);

    lv_obj_t* divider = lv_obj_create(root);
    lv_obj_remove_style_all(divider);
    lv_obj_add_style(divider, &style_alarm_divider, LV_STATE_DEFAULT);
    lv_obj_set_size(divider, 1, LV_PCT(88)); /* 竖线略矮于栏高, 两端留白更柔和 */

    lv_obj_t* col_right = brtvol_col_create(root);
    lv_obj_set_flex_grow(col_right, 1);

    /* 每秒刷新顶栏 WiFi */
    lv_timer_create(bt_check_timer_cb, 1000, NULL);
    return root;
}


void bt_slider_style_init(void) {
    if (bt_slider_style_inited)
        return;
    bt_slider_style_inited = true;

    /* 轨道: 6px 高圆角条, 深蓝紫 (T113 纯色渲染便宜) */
    lv_style_init(&style_bt_slider_main);
    lv_style_set_bg_opa(&style_bt_slider_main, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bt_slider_main, lv_color_hex(0x2E3690));
    lv_style_set_radius(&style_bt_slider_main, LV_RADIUS_CIRCLE);
    /* 不对称 padding:
       左 0: 指示条从轨道左缘开始, 100% 时左侧填满;
       右 12 (= knob 半径): LVGL v9 的 knob 中心对齐指示条右端,
       100% 时 knob 右缘正好贴住轨道右缘, 不再超出被卡片截断。
       代价: 0% 时 knob 向左悬出 12px(亮度 0 屏幕已暗, 可接受) */
    lv_style_set_pad_left(&style_bt_slider_main, 0);
    lv_style_set_pad_right(&style_bt_slider_main, 12);
    lv_style_set_pad_top(&style_bt_slider_main, 0);
    lv_style_set_pad_bottom(&style_bt_slider_main, 0);

    /* 已滑部分: 亮蓝紫 */
    lv_style_init(&style_bt_slider_ind);
    lv_style_set_bg_opa(&style_bt_slider_ind, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bt_slider_ind, lv_color_hex(0x6B7BFF));
    lv_style_set_radius(&style_bt_slider_ind, LV_RADIUS_CIRCLE);

    /* 圆钮: 白底蓝边, 半径全圆 */
    lv_style_init(&style_bt_slider_knob);
    lv_style_set_bg_opa(&style_bt_slider_knob, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bt_slider_knob, lv_color_white());
    lv_style_set_border_width(&style_bt_slider_knob, 2);
    lv_style_set_border_color(&style_bt_slider_knob, lv_color_hex(0x6B7BFF));
    lv_style_set_radius(&style_bt_slider_knob, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&style_bt_slider_knob, 0);
}
