#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "page_conf.h"
#include "res_conf.h"
#include "res/music/music_conf.h"

/* 顶栏动态元素: 时间 label + WiFi 图标/文案 (连接状态实时切换) */
static lv_obj_t* g_top_time_label = NULL;
static lv_obj_t* g_top_wifi_icon = NULL;
static lv_obj_t* g_top_wifi_label = NULL;
/* 上次顶栏 WiFi 状态 (状态没变不刷新). 初值 true: 顶栏创建时写死"已连接",
 * 真实状态未连接时首次轮询才会切到白色图标, 避免启动瞬间误跳 */
static bool g_top_wifi_connected = true;

/* wpa_manager 事件线程写, 主循环轮询读 (只写标志位, 线程安全) */
static volatile WPA_WIFI_CONNECT_STATUS_E g_conn_status = WPA_WIFI_INACTIVE;

/* 顶栏时钟回调 (定义在 top_component_add_info 之后) */
static void top_clock_timer_cb(lv_timer_t* t);

// 封装一个图标显示函数
static lv_obj_t* lv_image_vewer_create(lv_obj_t* parent, const char* image_path, lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, image_path);
    lv_obj_align(img, align, x_ofs, y_ofs);
    return img;
}

// 注册中间矩形组件: 闹钟管理卡片 (左半闹钟设置, 右半倒计时)
static lv_obj_t* lv_mid_screen_componnet_create(lv_obj_t* parent) {
    /* 状态卡片: 靠屏幕右半区 (卡片 60% 宽, 从右侧向内收, 不与左侧设置列表重叠) */
    lv_obj_t* sp_panel = status_panel_create(parent, LV_PCT(60), LV_PCT(60));
    lv_obj_align(sp_panel, LV_ALIGN_RIGHT_MID, -20, 0);

    alarm_manage_create(sp_panel); /* 闹钟管理组件填充卡片 */

    return sp_panel;
}

/* ============ 顶部组件: 第一部分 - 盒子创建 ============ */
/* 创建顶部背景图 + flex 图标容器, 返回容器供 add_info 使用 */
static lv_obj_t* top_component_create(lv_obj_t* parent) {
    lv_obj_t* top_img = lv_image_vewer_create(parent, IMAGE_PATH "top.png", LV_ALIGN_TOP_MID, 0, 0);

    /* 图标容器: flex 横向排布 */
    lv_obj_t* temp = lv_obj_create(top_img);
    lv_obj_remove_style_all(temp);
    lv_obj_set_size(temp, LV_PCT(50), LV_PCT(100));
    lv_obj_set_style_layout(temp, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);      /* 开启 flex */
    lv_obj_set_style_flex_flow(temp, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT); /* 横向 */
    lv_obj_align(temp, LV_ALIGN_CENTER, 0, 0);

    return temp; /* 注意: 返回的是 flex 容器, 不是背景图 */
}

/* ============ 顶部组件: 第二部分 - 显示信息添加 ============ */
typedef struct {
    const char* icon_path; /* 图标图片路径 */
    const char* text;      /* 文案 */
} top_info_item_t;

/* 往顶部容器里添加 [图标 + 文案] 条目, 每个 flex_grow(1) 均分容器宽度 */
static void top_component_add_info(lv_obj_t* container) {
    static const top_info_item_t items[] = {
        {IMAGE_PATH "iconfont_shijian.png", "时间:13:45"},
        {IMAGE_PATH "iconfont_wifi.png", "WIFI:已连接"},
        {IMAGE_PATH "iconfont_bule.png", "蓝牙:已连接"},
        {IMAGE_PATH "iconfont_yinl.png", "音量:50%"},
    };

    lv_font_t* font = get_font(FONT_TYPE_CN, 16); /* 顶栏文案字体 (中文必需) */
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        lv_obj_t* box = lv_obj_create(container);
        lv_obj_remove_style_all(box);
        lv_obj_set_size(box, LV_PCT(20), LV_PCT(100));

        lv_obj_t* icon = lv_image_vewer_create(box, items[i].icon_path, LV_ALIGN_LEFT_MID, 80, 0);
        lv_coord_t src_w = lv_image_get_src_width(icon); /* 原图是 200x200 大图, 按宽度缩到 16px */
        if (src_w > 0) {
            lv_image_set_scale(icon, 256 * 16 / src_w);
        }
        lv_obj_t* label = lv_label_create(box);
        lv_label_set_text(label, items[i].text);
        if (font != NULL) {
            lv_obj_set_style_text_font(label, font, LV_STATE_DEFAULT);
        }
        lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT); /* 默认黑色字在深色栏上看不见 */
        lv_obj_align_to(label, icon, LV_ALIGN_LEFT_MID, 30, 0);

        /* 动态元素: 记住指针, 供状态变化时更新 (时间实时刷新 / WiFi 图标切换) */
        if (strncmp(items[i].text, "时间:", 6) == 0) {
            g_top_time_label = label;
        } else if (strncmp(items[i].text, "WIFI:", 5) == 0) {
            g_top_wifi_icon = icon;
            g_top_wifi_label = label;
        }

        lv_obj_set_flex_grow(box, 1); /* 均分的关键 */
    }

    /* 顶栏时钟: 每秒刷新 "时间:HH:MM" */
    lv_timer_create(top_clock_timer_cb, 1000, NULL);
}

/* 顶栏时钟: 每秒更新时间显示 */
static void top_clock_timer_cb(lv_timer_t* t) {
    if (g_top_time_label == NULL)
        return;
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    char buf[24];
    snprintf(buf, sizeof(buf), "时间:%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
    lv_label_set_text(g_top_time_label, buf);
}

/* 顶栏 WiFi 图标/文案: 连接成功=蓝色图标, 未连接=白色图标 (状态没变不刷新) */
static void update_top_wifi_ui(void) {
    if (g_top_wifi_icon == NULL)
        return;
    bool conn = (g_conn_status == WPA_WIFI_CONNECT);
    if (conn == g_top_wifi_connected)
        return;
    g_top_wifi_connected = conn;
    lv_image_set_src(g_top_wifi_icon,
                     conn ? IMAGE_PATH "iconfont_wifi.png"       /* 蓝: 已连接 */
                          : IMAGE_PATH "iconfont_wifi_off.png"); /* 白: 未连接 */
    lv_label_set_text(g_top_wifi_label, conn ? "WIFI:已连接" : "WIFI:未连接");
}

/* wpa_manager 事件线程回调 → 只写标志位, 绝不操作 LVGL (非线程安全) */
void wifi_status_ui_cb(WPA_WIFI_CONNECT_STATUS_E status) {
    g_conn_status = status;
}

/* ==================== 闹钟管理组件 (左半: 闹钟设置, 右半: 倒计时) ==================== */

/* 响铃: 板上 aplay 后台播放 (shell 的 & 立即返回, 不卡 UI); 模拟器跳过并打印 */
static void alarm_play_sound(const char* path) {
#ifdef SIMULATOR_LINUX
    printf("[alarm] 模拟器跳过播放: %s\n", path);
    fflush(stdout);
#else
    char cmd[192];
    snprintf(cmd, sizeof(cmd), "killall aplay 2>/dev/null; aplay %s &", path);
    system(cmd);
#endif
}

/* 生成 "00\n01\n...\n<max>" roller 选项串, 索引即数值 (index==value) */
static void build_roller_options(char* buf, int max_val) {
    char* p = buf;
    for (int i = 0; i <= max_val; i++)
        p += sprintf(p, "%s%02d", i ? "\n" : "", i);
}

/* ---------- 闹钟设置 (左栏) ---------- */
static lv_obj_t* g_alarm_big_label = NULL;  /* 大字 "HH:MM" */
static lv_obj_t* g_alarm_switch = NULL;     /* 开/关胶囊按钮 */
static lv_obj_t* g_alarm_roller_h = NULL;   /* 小时 roller (00-23) */
static lv_obj_t* g_alarm_roller_m = NULL;   /* 分钟 roller (00-59) */
static bool g_alarm_fired = false;          /* 本周期是否已响过铃 (edge 触发) */

/* roller 变更 → 刷新大字时间 + 允许重新到点响铃 */
static void alarm_roller_change_cb(lv_event_t* e) {
    int h = (int)lv_roller_get_selected(g_alarm_roller_h);
    int m = (int)lv_roller_get_selected(g_alarm_roller_m);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    lv_label_set_text(g_alarm_big_label, buf);
    g_alarm_fired = false;
}

/* 开关点击: CHECKED 态取反, 文案 开/关 同步 */
static void alarm_switch_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    if (lv_obj_has_state(btn, LV_STATE_CHECKED))
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    else
        lv_obj_add_state(btn, LV_STATE_CHECKED);
    lv_obj_t* label = lv_event_get_user_data(e);
    lv_label_set_text(label, lv_obj_has_state(btn, LV_STATE_CHECKED) ? "开" : "关");
}

/* 创建左栏: 大字时间 + 开关 + 时/分 roller 横条 */
static lv_obj_t* alarm_col_left_create(lv_obj_t* parent) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(col, false);

    /* 大字时间 (40px 中文 TTF, 数字高度充裕) */
    g_alarm_big_label = lv_label_create(col);
    lv_font_t* font_big = get_font(FONT_TYPE_CN, 40);
    if (font_big != NULL)
        lv_obj_set_style_text_font(g_alarm_big_label, font_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_alarm_big_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(g_alarm_big_label, "07:00");
    lv_obj_align(g_alarm_big_label, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 开关胶囊: 绝对定位右下角, 与右栏按钮行同处底部 */
    lv_obj_t* sw = lv_btn_create(col);
    lv_obj_remove_style_all(sw);
    lv_obj_add_style(sw, &style_alarm_switch, LV_STATE_DEFAULT);
    lv_obj_add_style(sw, &style_alarm_switch_checked, LV_STATE_CHECKED);
    lv_obj_set_size(sw, 64, 24);
    lv_obj_align(sw, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_scrollable(sw, false);
    lv_obj_t* sw_label = lv_label_create(sw);
    lv_font_t* font_sw = get_font(FONT_TYPE_CN, 14);
    if (font_sw != NULL)
        lv_obj_set_style_text_font(sw_label, font_sw, LV_STATE_DEFAULT);
    lv_label_set_text(sw_label, "关");
    lv_obj_center(sw_label);
    lv_obj_add_event_cb(sw, alarm_switch_cb, LV_EVENT_CLICKED, sw_label);
    g_alarm_switch = sw;

    /* 时:分 roller 横条 (flex row 居中) */
    lv_obj_t* bar = lv_obj_create(col);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 142, 57);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_layout(bar, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(bar, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(bar, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    char opts[256];
    build_roller_options(opts, 23);
    g_alarm_roller_h = lv_roller_create(bar);
    lv_obj_remove_style_all(g_alarm_roller_h);
    lv_obj_add_style(g_alarm_roller_h, &style_alarm_roller, LV_STATE_DEFAULT);
    lv_obj_add_style(g_alarm_roller_h, &style_alarm_roller_sel, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_font_t* font_r = get_font(FONT_TYPE_CN, 14); /* 必须先设字体, set_visible_row_count 按字体算高 */
    if (font_r != NULL)
        lv_obj_set_style_text_font(g_alarm_roller_h, font_r, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_roller_set_options(g_alarm_roller_h, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_alarm_roller_h, 3);
    lv_roller_set_selected(g_alarm_roller_h, 7, LV_ANIM_OFF); /* 默认 07 */
    lv_obj_set_width(g_alarm_roller_h, 64);
    lv_obj_set_style_text_align(g_alarm_roller_h, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_alarm_roller_h, alarm_roller_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* colon = lv_label_create(bar);
    lv_font_t* font_c = get_font(FONT_TYPE_CN, 14);
    if (font_c != NULL)
        lv_obj_set_style_text_font(colon, font_c, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(colon, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_pad_left(colon, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(colon, 2, LV_STATE_DEFAULT);

    build_roller_options(opts, 59);
    g_alarm_roller_m = lv_roller_create(bar);
    lv_obj_remove_style_all(g_alarm_roller_m);
    lv_obj_add_style(g_alarm_roller_m, &style_alarm_roller, LV_STATE_DEFAULT);
    lv_obj_add_style(g_alarm_roller_m, &style_alarm_roller_sel, LV_PART_SELECTED | LV_STATE_DEFAULT);
    if (font_r != NULL)
        lv_obj_set_style_text_font(g_alarm_roller_m, font_r, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_roller_set_options(g_alarm_roller_m, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_alarm_roller_m, 3);
    lv_roller_set_selected(g_alarm_roller_m, 0, LV_ANIM_OFF); /* 默认 00 */
    lv_obj_set_width(g_alarm_roller_m, 64);
    lv_obj_set_style_text_align(g_alarm_roller_m, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_alarm_roller_m, alarm_roller_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return col;
}

/* ---------- 倒计时 (右栏) ---------- */
typedef enum { CD_IDLE, CD_RUNNING, CD_PAUSED } cd_state_t;

static lv_obj_t* g_cd_label = NULL;          /* 大字 "MM:SS" */
static lv_obj_t* g_cd_roller_m = NULL;       /* 分钟 roller (00-59) */
static lv_obj_t* g_cd_roller_s = NULL;       /* 秒 roller (00-59) */
static lv_obj_t* g_cd_btn_start = NULL;      /* 开始/继续 */
static lv_obj_t* g_cd_btn_pause = NULL;      /* 暂停 */
static lv_obj_t* g_cd_btn_reset = NULL;      /* 重置 */
static lv_obj_t* g_cd_btn_start_label = NULL;
static lv_obj_t* g_cd_btn_pause_label = NULL;
static cd_state_t g_cd_state = CD_IDLE;
static int32_t g_cd_total = 0;      /* 设定总秒数 (重置时恢复) */
static int32_t g_cd_remaining = 0;  /* 剩余秒数 */

/* 倒计时 roller 变更 → 大字实时预览设定值 (仅 IDLE 可调, 运行中 roller 已禁用) */
static void cd_roller_change_cb(lv_event_t* e) {
    (void)e;
    int m = (int)lv_roller_get_selected(g_cd_roller_m);
    int s = (int)lv_roller_get_selected(g_cd_roller_s);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    lv_label_set_text(g_cd_label, buf);
}

/* 状态切换后统一刷新: 大字 label + 三个按钮的禁用态/文案 */
static void cd_update_ui(void) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", (int)(g_cd_remaining / 60), (int)(g_cd_remaining % 60));
    lv_label_set_text(g_cd_label, buf);

    bool idle = (g_cd_state == CD_IDLE);
    bool running = (g_cd_state == CD_RUNNING);
    lv_obj_t* m = g_cd_roller_m;
    lv_obj_t* s = g_cd_roller_s;

    /* 开始/继续: IDLE 可用, RUNNING/PAUSED 禁用 */
    lv_obj_set_state(g_cd_btn_start, LV_STATE_DISABLED, running);
    lv_label_set_text(g_cd_btn_start_label, (g_cd_state == CD_PAUSED) ? "继续" : "开始");
    /* 暂停: 仅 RUNNING 可用 */
    lv_obj_set_state(g_cd_btn_pause, LV_STATE_DISABLED, !running);
    lv_label_set_text(g_cd_btn_pause_label, (g_cd_state == CD_PAUSED) ? "已暂停" : "暂停");
    /* 重置: IDLE 也可用 (回到设定时长) */
    lv_obj_set_state(g_cd_btn_reset, LV_STATE_DISABLED, false);
    /* roller: 仅 IDLE 可调 */
    lv_obj_set_state(m, LV_STATE_DISABLED, !idle);
    lv_obj_set_state(s, LV_STATE_DISABLED, !idle);
}

/* 开始/继续 */
static void cd_btn_start_cb(lv_event_t* e) {
    if (g_cd_state == CD_RUNNING)
        return;
    if (g_cd_state == CD_IDLE) {
        int m = (int)lv_roller_get_selected(g_cd_roller_m);
        int s = (int)lv_roller_get_selected(g_cd_roller_s);
        g_cd_total = m * 60 + s;
        if (g_cd_total <= 0)
            return; /* 0 秒不允许开始 */
        g_cd_remaining = g_cd_total;
    }
    g_cd_state = CD_RUNNING;
    cd_update_ui();
}

/* 暂停 */
static void cd_btn_pause_cb(lv_event_t* e) {
    if (g_cd_state != CD_RUNNING)
        return;
    g_cd_state = CD_PAUSED;
    cd_update_ui();
}

/* 重置 */
static void cd_btn_reset_cb(lv_event_t* e) {
    g_cd_state = CD_IDLE;
    g_cd_remaining = g_cd_total;
    cd_update_ui();
}

/* 创建右栏: 大字剩余时间 + 分/秒 roller + 三个按钮 */
static lv_obj_t* alarm_col_right_create(lv_obj_t* parent) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(col, false);

    /* 大字剩余时间 */
    g_cd_label = lv_label_create(col);
    lv_font_t* font_big = get_font(FONT_TYPE_CN, 40);
    if (font_big != NULL)
        lv_obj_set_style_text_font(g_cd_label, font_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_cd_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(g_cd_label, "05:00");
    lv_obj_align(g_cd_label, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 分:秒 roller 横条: 靠左下 */
    lv_obj_t* bar = lv_obj_create(col);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 142, 57);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_layout(bar, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(bar, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(bar, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    char opts[256];
    build_roller_options(opts, 59);
    g_cd_roller_m = lv_roller_create(bar);
    lv_obj_remove_style_all(g_cd_roller_m);
    lv_obj_add_style(g_cd_roller_m, &style_alarm_roller, LV_STATE_DEFAULT);
    lv_obj_add_style(g_cd_roller_m, &style_alarm_roller_sel, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_font_t* font_r = get_font(FONT_TYPE_CN, 14); /* 必须先设字体 */
    if (font_r != NULL)
        lv_obj_set_style_text_font(g_cd_roller_m, font_r, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_roller_set_options(g_cd_roller_m, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_cd_roller_m, 3);
    lv_roller_set_selected(g_cd_roller_m, 5, LV_ANIM_OFF); /* 默认 05 */
    lv_obj_set_width(g_cd_roller_m, 64);
    lv_obj_set_style_text_align(g_cd_roller_m, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_cd_roller_m, cd_roller_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* colon = lv_label_create(bar);
    lv_font_t* font_c = get_font(FONT_TYPE_CN, 14);
    if (font_c != NULL)
        lv_obj_set_style_text_font(colon, font_c, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(colon, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_pad_left(colon, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(colon, 2, LV_STATE_DEFAULT);

    g_cd_roller_s = lv_roller_create(bar);
    lv_obj_remove_style_all(g_cd_roller_s);
    lv_obj_add_style(g_cd_roller_s, &style_alarm_roller, LV_STATE_DEFAULT);
    lv_obj_add_style(g_cd_roller_s, &style_alarm_roller_sel, LV_PART_SELECTED | LV_STATE_DEFAULT);
    if (font_r != NULL)
        lv_obj_set_style_text_font(g_cd_roller_s, font_r, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_roller_set_options(g_cd_roller_s, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_cd_roller_s, 3);
    lv_roller_set_selected(g_cd_roller_s, 0, LV_ANIM_OFF); /* 默认 00 */
    lv_obj_set_width(g_cd_roller_s, 64);
    lv_obj_set_style_text_align(g_cd_roller_s, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_cd_roller_s, cd_roller_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 三个小按钮: 靠右下 */
    lv_obj_t* btn_row = lv_obj_create(col);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, 226, 24);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_layout(btn_row, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(btn_row, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_main_place(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(btn_row, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    lv_font_t* font_btn = get_font(FONT_TYPE_CN, 14);

    g_cd_btn_start = lv_btn_create(btn_row);
    lv_obj_remove_style_all(g_cd_btn_start);
    lv_obj_add_style(g_cd_btn_start, &style_alarm_small_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(g_cd_btn_start, &style_alarm_small_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(g_cd_btn_start, &style_alarm_small_btn_disabled, LV_STATE_DISABLED);
    lv_obj_set_size(g_cd_btn_start, 70, 24);
    lv_obj_set_scrollable(g_cd_btn_start, false);
    g_cd_btn_start_label = lv_label_create(g_cd_btn_start);
    if (font_btn != NULL)
        lv_obj_set_style_text_font(g_cd_btn_start_label, font_btn, LV_STATE_DEFAULT);
    lv_label_set_text(g_cd_btn_start_label, "开始");
    lv_obj_center(g_cd_btn_start_label);
    lv_obj_add_event_cb(g_cd_btn_start, cd_btn_start_cb, LV_EVENT_CLICKED, NULL);

    g_cd_btn_pause = lv_btn_create(btn_row);
    lv_obj_remove_style_all(g_cd_btn_pause);
    lv_obj_add_style(g_cd_btn_pause, &style_alarm_small_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(g_cd_btn_pause, &style_alarm_small_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(g_cd_btn_pause, &style_alarm_small_btn_disabled, LV_STATE_DISABLED);
    lv_obj_set_size(g_cd_btn_pause, 70, 24);
    lv_obj_set_scrollable(g_cd_btn_pause, false);
    g_cd_btn_pause_label = lv_label_create(g_cd_btn_pause);
    if (font_btn != NULL)
        lv_obj_set_style_text_font(g_cd_btn_pause_label, font_btn, LV_STATE_DEFAULT);
    lv_label_set_text(g_cd_btn_pause_label, "暂停");
    lv_obj_center(g_cd_btn_pause_label);
    lv_obj_add_event_cb(g_cd_btn_pause, cd_btn_pause_cb, LV_EVENT_CLICKED, NULL);

    g_cd_btn_reset = lv_btn_create(btn_row);
    lv_obj_remove_style_all(g_cd_btn_reset);
    lv_obj_add_style(g_cd_btn_reset, &style_alarm_small_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(g_cd_btn_reset, &style_alarm_small_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(g_cd_btn_reset, &style_alarm_small_btn_disabled, LV_STATE_DISABLED);
    lv_obj_set_size(g_cd_btn_reset, 70, 24);
    lv_obj_set_scrollable(g_cd_btn_reset, false);
    lv_obj_t* reset_label = lv_label_create(g_cd_btn_reset);
    if (font_btn != NULL)
        lv_obj_set_style_text_font(reset_label, font_btn, LV_STATE_DEFAULT);
    lv_label_set_text(reset_label, "重置");
    lv_obj_center(reset_label);
    lv_obj_add_event_cb(g_cd_btn_reset, cd_btn_reset_cb, LV_EVENT_CLICKED, NULL);

    /* 初始: IDLE, 暂停/重置禁用 */
    g_cd_total = 5 * 60;
    g_cd_remaining = g_cd_total;
    lv_obj_set_state(g_cd_btn_pause, LV_STATE_DISABLED, true);
    lv_obj_set_state(g_cd_btn_reset, LV_STATE_DISABLED, true);
    lv_obj_set_state(g_cd_btn_start, LV_STATE_DISABLED, false);

    return col;
}

/* 每秒: 顶栏 WiFi 轮询 + 闹钟到点比对 + 倒计时 tick */
static void alarm_check_timer_cb(lv_timer_t* t) {
    /* 顶栏 WiFi 状态 (原 wifi_conn_poll_cb 的职责, 1s 粒度够) */
    update_top_wifi_ui();

    /* ---- 闹钟到点比对 (edge 触发, 防每秒重复响) ---- */
    if (g_alarm_switch != NULL && lv_obj_has_state(g_alarm_switch, LV_STATE_CHECKED)) {
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        int set_h = (int)lv_roller_get_selected(g_alarm_roller_h);
        int set_m = (int)lv_roller_get_selected(g_alarm_roller_m);
        if (tm_now.tm_hour == set_h && tm_now.tm_min == set_m) {
            if (!g_alarm_fired) {
                g_alarm_fired = true;
                alarm_play_sound(GET_MUSIC_PATH("audio_warn.wav"));
            }
        } else {
            g_alarm_fired = false; /* 时间错开, 允许下个周期再响 */
        }
    }

    /* ---- 倒计时 tick ---- */
    if (g_cd_state == CD_RUNNING) {
        g_cd_remaining--;
        if (g_cd_remaining <= 0) {
            alarm_play_sound(GET_MUSIC_PATH("audio_finish.wav"));
            g_cd_state = CD_IDLE;
            g_cd_remaining = g_cd_total;
        }
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", (int)(g_cd_remaining / 60), (int)(g_cd_remaining % 60));
        lv_label_set_text(g_cd_label, buf);
        if (g_cd_state != CD_RUNNING)
            cd_update_ui(); /* 结束时复位按钮/roller */
    }
}

/* 闹钟管理组件: 卡片内左右分栏, 左=闹钟设置, 右=倒计时 */
lv_obj_t* alarm_manage_create(lv_obj_t* parent) {
    alarm_manage_style_init();

    /* 根容器: flex row 两栏 + 1px 竖分隔线 */
    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_layout(root, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(root, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(root, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    lv_obj_t* col_left = alarm_col_left_create(root);
    lv_obj_set_flex_grow(col_left, 1);

    lv_obj_t* divider = lv_obj_create(root);
    lv_obj_remove_style_all(divider);
    lv_obj_add_style(divider, &style_alarm_divider, LV_STATE_DEFAULT);
    lv_obj_set_size(divider, 1, LV_PCT(88)); /* 竖线略矮于栏高, 两端留白更柔和 */

    lv_obj_t* col_right = alarm_col_right_create(root);
    lv_obj_set_flex_grow(col_right, 1);

    /* 每秒刷新: 顶栏 WiFi + 闹钟到点 + 倒计时 */
    lv_timer_create(alarm_check_timer_cb, 1000, NULL);
    return root;
}

void set_init(void) {
    font_init(); /* 注册 TTF 字体路径, 必须在任何 get_font() 之前 */

    lv_image_vewer_create(lv_screen_active(), IMAGE_PATH "back.png", LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t* top_box = top_component_create(lv_screen_active()); /* 第一部分: 盒子 */
    top_component_add_info(top_box);                              /* 第二部分: 显示信息 */

    lv_obj_t* bottom_img = bottom_bar_create(lv_screen_active()); /* 程序化底部栏, 替换 bottom.png */

    lv_obj_t* panel = settings_panel_create(lv_scr_act(), LV_PCT(20), LV_PCT(65));
    lv_obj_align_to(panel, lv_scr_act(), LV_ALIGN_LEFT_MID, 107, 0);
    lv_obj_update_layout(panel);

    lv_font_t* font = get_font(FONT_TYPE_CN, 10);
    if (font != NULL) {
        lv_obj_set_style_text_font(panel, font, LV_STATE_DEFAULT);
    }
    /* TODO: 把你的图标 PNG 放到 res/image/ 下, 把 NULL 换成 IMAGE_PATH "你的图标.png" */
    settings_row_t row_bt = row_add_clickable(panel, IMAGE_PATH "iconfont_bule.png", "蓝牙设置", "显示与亮度");
    settings_row_t row_disp = row_add_clickable(panel, IMAGE_PATH "wangl.png", "网络连接 ", "wifi连接");
    settings_row_t row_update = row_add_clickable(panel, IMAGE_PATH "iconfont_genx.png", "网络设置", "蓝牙设置");
    settings_row_t row_alarm = row_add_clickable(panel, IMAGE_PATH "iconfont_laoz.png", "闹钟管理", "闹钟管理");
    settings_row_t row_log = row_add_clickable(panel, IMAGE_PATH "iconfont_riz.png", "日志记录", "系统更新");

    settings_row_set_selected(&row_alarm, true); /* 初始选中第 3 行 */

    lv_obj_t* sp_panel = lv_mid_screen_componnet_create(lv_screen_active());
    lv_obj_align_to(sp_panel, panel, LV_ALIGN_OUT_RIGHT_MID, 66, 0);

    lv_obj_t* btn = bottom_bar_btn_create(bottom_img, "返回"); /* 深蓝玻璃, 与底部栏同色系 */

    /* 字体设在按钮对象上, 经继承链传给内部文案 label */
    lv_font_t* font_btn = get_font(FONT_TYPE_CN, 30);
    if (font_btn != NULL) {
        lv_obj_set_style_text_font(btn, font_btn, LV_STATE_DEFAULT);
    }
}
