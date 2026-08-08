#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "page_conf.h"
#include "res_conf.h"

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

// 注册中间矩形组件: 蓝牙/亮度/音量管理卡片 (左半蓝牙摆设, 右半亮度/音量 slider)
static lv_obj_t* lv_mid_screen_componnet_create(lv_obj_t* parent) {
    /* 状态卡片: 靠屏幕右半区 (卡片 60% 宽, 从右侧向内收, 不与左侧设置列表重叠) */
    lv_obj_t* sp_panel = status_panel_create(parent, LV_PCT(60), LV_PCT(60));
    lv_obj_align(sp_panel, LV_ALIGN_RIGHT_MID, -20, 0);

    bt_setting_create(sp_panel); /* 蓝牙/亮度/音量组件填充卡片 */

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

/* ==================== 蓝牙 / 亮度 / 音量管理组件 ==================== */

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

/* 音量 slider: 同亮度, 松手写 amixer */
static void vol_slider_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", val);
    lv_label_set_text(g_vol_label, buf);
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

    /* 下半: 音量 (默认 80%) */
    lv_obj_t* half2 = lv_obj_create(col);
    lv_obj_remove_style_all(half2);
    lv_obj_set_size(half2, LV_PCT(100), LV_PCT(50));
    lv_obj_align(half2, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_scrollable(half2, false);
    brtvol_row_create(half2, "音量", 80, &g_vol_label, vol_slider_cb);

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
