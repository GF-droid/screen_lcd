#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "page_conf.h"
#include "res_conf.h"

/*
 * demo7 主页面: 一条超宽横向玻璃面板 (1424x280 屏幕, 面板 1320x232)
 *
 *   时间日期区 | 天气卡片 | 分隔线 | 功能卡片组
 *   底部: 波浪缺口 + 提示条 (夜间出行请注意添衣)
 *
 * 数据来源: 时钟=本地时间, 农历=内嵌年表 (1900-2099, 逐日验证过),
 * WIFI=wpa_manager 状态回调, 温湿度=模拟值 (板上无传感器), 亮度=背光 sysfs。
 */

/* ==================== 农历年表 (1900-2099, 来源 lunardate 库, 逐日验证) ====================
 * 编码: bit(16-m) = 第 m 月大小 (1=30天); bit0-3 = 闰月号 (0=无闰);
 *       bit16 = 闰月大小 (1=30天) */
static const uint32_t s_lunar_info[200] = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0,
    0x09ad0, 0x055d2, 0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540,
    0x0d6a0, 0x0ada2, 0x095b0, 0x14977, 0x04970, 0x0a4b0, 0x0b4b5, 0x06a50,
    0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970, 0x06566, 0x0d4a0,
    0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2,
    0x0a950, 0x0b557, 0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5d0, 0x14573,
    0x052b0, 0x0a9a8, 0x0e950, 0x06aa0, 0x0aea6, 0x0ab50, 0x04b60, 0x0aae4,
    0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0, 0x096d0, 0x04dd5,
    0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b5a0, 0x195a6,
    0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46,
    0x0ab60, 0x09570, 0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58,
    0x05ac0, 0x0ab60, 0x096d5, 0x092e0, 0x0c960, 0x0d954, 0x0d4a0, 0x0da50,
    0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5, 0x0a950, 0x0b4a0,
    0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260,
    0x0ea65, 0x0d530, 0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0,
    0x1d0b6, 0x0d250, 0x0d520, 0x0dd45, 0x0b5a0, 0x056d0, 0x055b2, 0x049b0,
    0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0, 0x14b63, 0x09370,
    0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06aa0, 0x1a6c4, 0x0aae0,
    0x092e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0,
    0x0a6d0, 0x055d4, 0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50,
    0x055a0, 0x0aba4, 0x0a5b0, 0x052b0, 0x0b273, 0x06930, 0x07337, 0x06aa0,
    0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d160, 0x0e968, 0x0d520,
    0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252,
};

static int lunar_leap_month(int y)       { return s_lunar_info[y - 1900] & 0x0F; }
static int lunar_leap_days(int y)        { return (s_lunar_info[y - 1900] & 0x10000) ? 30 : 29; }
static int lunar_month_days(int y, int m){ return (s_lunar_info[y - 1900] & (1 << (16 - m))) ? 30 : 29; }
static int lunar_year_days(int y) {
    int n = 0;
    for (int m = 1; m <= 12; m++)
        n += lunar_month_days(y, m);
    if (lunar_leap_month(y))
        n += lunar_leap_days(y);
    return n;
}

/* 公历 -> 农历 (1900-2099). 1900-01-31 = 农历 1900 年正月初一 */
static void solar_to_lunar(int sy, int sm, int sd, int* ly, int* lm, int* ld, int* is_leap) {
    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int offset = 0;
    for (int y = 1900; y < sy; y++) {
        offset += ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 366 : 365;
    }
    for (int m = 1; m < sm; m++) {
        offset += mdays[m - 1];
        if (m == 2 && ((sy % 4 == 0 && sy % 100 != 0) || sy % 400 == 0))
            offset++;
    }
    offset += sd - 31; /* 1900-01-31 是第 0 天 */

    int y = 1900;
    while (y < 2100 && offset >= lunar_year_days(y)) {
        offset -= lunar_year_days(y);
        y++;
    }
    int leap = lunar_leap_month(y);
    for (int m = 1; m <= 12; m++) {
        if (offset < lunar_month_days(y, m)) {
            *ly = y; *lm = m; *ld = offset + 1; *is_leap = 0; return;
        }
        offset -= lunar_month_days(y, m);
        /* 闰月紧跟第 m 月之后 */
        if (leap && m == leap) {
            if (offset < lunar_leap_days(y)) {
                *ly = y; *lm = m; *ld = offset + 1; *is_leap = 1; return;
            }
            offset -= lunar_leap_days(y);
        }
    }
    /* 正常不会到这里 */
    *ly = y; *lm = 1; *ld = 1; *is_leap = 0;
}

static const char* const s_week_cn[] = {"星期日", "星期一", "星期二", "星期三",
                                        "星期四", "星期五", "星期六"};
static const char* const s_lunar_month_cn[] = {"正", "二", "三", "四", "五", "六",
                                               "七", "八", "九", "十", "冬", "腊"};
static const char* const s_lunar_day_cn[] = {
    "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
    "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
    "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"};
static const char* const s_gan[] = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
static const char* const s_zhi[] = {"子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};

/* ==================== 页面元素句柄 ==================== */
static lv_obj_t* g_clock_label = NULL;    /* 大时钟 22:48 */
static lv_obj_t* g_date_label = NULL;     /* 星期 + 公历 + 干支农历一行 */
static lv_obj_t* g_temp_label = NULL;     /* 室内温度 */
static lv_obj_t* g_humi_label = NULL;     /* 室内湿度 */
static lv_obj_t* g_wifi_icon = NULL;      /* WIFI 卡图标 */
static lv_obj_t* g_wifi_state_label = NULL; /* WIFI 卡状态文字 */
static lv_obj_t* g_bright_state_label = NULL; /* 亮度卡状态文字 */
static lv_obj_t* g_tip_label = NULL;      /* 提示条文字 */

/* wpa_manager 事件线程写, 主循环定时器轮询读 (只写标志位, 线程安全)
 * g_conn_status / wifi_status_ui_cb 定义在 page_setting.c (设置页共用同一份) */
extern volatile WPA_WIFI_CONNECT_STATUS_E g_conn_status;

/* 主页面 screen 句柄 (设置页"返回"用它切回来; 定义在 page_conf.h 声明) */
lv_obj_t* g_home_screen = NULL;

void setting_screen_show(void) {
    lv_screen_load(g_setting_screen);
}
void home_screen_show(void) {
    lv_screen_load(g_home_screen);
}

/* ==================== 小工具 ==================== */
static lv_font_t* s_font(int size) {
    return get_font(FONT_TYPE_CN, size);
}

static lv_obj_t* img_create(lv_obj_t* parent, const char* path) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, path);
    return img;
}

static lv_obj_t* label_create(lv_obj_t* parent, int size, lv_color_t color) {
    lv_obj_t* lb = lv_label_create(parent);
    lv_font_t* f = s_font(size);
    if (f)
        lv_obj_set_style_text_font(lb, f, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lb, color, LV_STATE_DEFAULT);
    return lb;
}

/* WIFI 状态 -> 卡面文字 */
static const char* wifi_state_text(void) {
    switch (g_conn_status) {
    case WPA_WIFI_CONNECT:    return "已连接";
    case WPA_WIFI_WRONG_KEY:  return "密码错误";
    case WPA_WIFI_SCANNING:   return "扫描中";
    case WPA_WIFI_DISCONNECT: return "未连接";
    default:                  return "未连接";
    }
}

/* 背光亮度百分比, 读失败返回 -1 */
static int read_brightness_pct(void) {
    static const char* const dirs[] = {
        "/sys/class/backlight/backlight",
        "/sys/class/backlight/backlight0",
        "/sys/class/backlight/sunxi-backlight",
    };
    char path[96];
    FILE* f = NULL;
    int bri = -1, max = -1;
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]) && bri < 0; i++) {
        snprintf(path, sizeof(path), "%s/brightness", dirs[i]);
        f = fopen(path, "r");
        if (f) {
            fscanf(f, "%d", &bri);
            fclose(f);
        }
    }
    if (bri < 0)
        return -1;
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]) && max < 0; i++) {
        snprintf(path, sizeof(path), "%s/max_brightness", dirs[i]);
        f = fopen(path, "r");
        if (f) {
            fscanf(f, "%d", &max);
            fclose(f);
        }
    }
    if (max <= 0)
        return -1;
    return bri * 100 / max;
}

/* ==================== 四区块创建 ==================== */

/* 1. 时间日期区 (面板内 x32..374) */
static void time_area_create(lv_obj_t* panel) {
    /* 超大时钟 */
    g_clock_label = label_create(panel, 64, lv_color_hex(0xE6ECFF));
    lv_obj_set_style_text_letter_space(g_clock_label, 6, LV_STATE_DEFAULT);
    lv_obj_align(g_clock_label, LV_ALIGN_TOP_LEFT, 32, 22);

    /* 细发光横线分割 */
    lv_obj_t* line = lv_obj_create(panel);
    lv_obj_remove_style_all(line);
    lv_obj_add_style(line, &style_glow_line, LV_STATE_DEFAULT);
    lv_obj_set_size(line, 180, 2);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 32, 116);

    /* 星期 + 公历 + 干支农历 */
    g_date_label = label_create(panel, 14, lv_color_hex(0x9DB4FF));
    lv_obj_align(g_date_label, LV_ALIGN_TOP_LEFT, 32, 136);

    /* 室内温湿度行 (家图标 + 温度 | 水滴图标 + 湿度) */
    lv_obj_t* home = img_create(panel, IMAGE_PATH "home.png");
    lv_obj_align(home, LV_ALIGN_TOP_LEFT, 32, 170);

    g_temp_label = label_create(panel, 14, lv_color_hex(0xE6ECFF));
    lv_obj_align(g_temp_label, LV_ALIGN_TOP_LEFT, 62, 169);

    lv_obj_t* sep = lv_obj_create(panel);
    lv_obj_remove_style_all(sep);
    lv_obj_add_style(sep, &style_glow_line, LV_STATE_DEFAULT);
    lv_obj_set_size(sep, 1, 16);
    lv_obj_align(sep, LV_ALIGN_TOP_LEFT, 122, 172);

    lv_obj_t* drop = img_create(panel, IMAGE_PATH "drop.png");
    lv_obj_align(drop, LV_ALIGN_TOP_LEFT, 134, 170);

    g_humi_label = label_create(panel, 14, lv_color_hex(0xE6ECFF));
    lv_obj_align(g_humi_label, LV_ALIGN_TOP_LEFT, 164, 169);
}

/* 2. 天气卡片 (面板内 x382..754, 独立圆角卡片) */
static void weather_area_create(lv_obj_t* panel) {
    lv_obj_t* card = lv_obj_create(panel);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &style_weather_card, LV_STATE_DEFAULT);
    lv_obj_set_size(card, 372, 150);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, 382, 38);

    /* 左边: 发光月亮 + 云朵 (青绿->淡紫渐变描边) */
    lv_obj_t* moon = img_create(card, IMAGE_PATH "moon_cloud.png");
    lv_obj_align(moon, LV_ALIGN_LEFT_MID, 14, 0);

    /* 右边: 大号温度 + 体感 + 天气概要 */
    lv_obj_t* t_big = label_create(card, 36, lv_color_white());
    lv_label_set_text(t_big, "24°");
    lv_obj_align(t_big, LV_ALIGN_TOP_LEFT, 158, 14);

    lv_obj_t* t_feel = label_create(card, 14, lv_color_hex(0xAAB8FF));
    lv_label_set_text(t_feel, "体感22°");
    lv_obj_align(t_feel, LV_ALIGN_TOP_LEFT, 160, 62);

    lv_obj_t* t_desc = label_create(card, 14, lv_color_hex(0x3CE0C8));
    lv_label_set_text(t_desc, "今日多云");
    lv_obj_align(t_desc, LV_ALIGN_TOP_LEFT, 160, 88);

    lv_obj_t* t_range = label_create(card, 14, lv_color_hex(0x9DB4FF));
    lv_label_set_text(t_range, "最高26° 最低18°");
    lv_obj_align(t_range, LV_ALIGN_TOP_LEFT, 160, 110);
}

/* 3. 竖直分隔线 + 中点发光方点 (面板内 x774) */
static void divider_create(lv_obj_t* panel) {
    lv_obj_t* line = lv_obj_create(panel);
    lv_obj_remove_style_all(line);
    lv_obj_add_style(line, &style_glow_line, LV_STATE_DEFAULT);
    lv_obj_set_size(line, 1, 120);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 774, 58);

    lv_obj_t* dot = lv_obj_create(panel);
    lv_obj_remove_style_all(dot);
    lv_obj_add_style(dot, &style_glow_dot, LV_STATE_DEFAULT);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 771, 114);
}

/* 4. 功能卡片组: 顶部发光短线 + 图标 + 标题 + 状态值 (面板内 x798..1288) */
static lv_obj_t* feature_card_create(lv_obj_t* panel, lv_coord_t x, const char* icon_path,
                                     const char* title, const char* state, bool wifi_card) {
    lv_obj_t* card = lv_obj_create(panel);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &style_feature_card, LV_STATE_DEFAULT);
    lv_obj_set_size(card, 100, 150);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, x, 38);

    /* 顶部居中发光短线 */
    lv_obj_t* top_line = lv_obj_create(card);
    lv_obj_remove_style_all(top_line);
    lv_obj_add_style(top_line, &style_glow_line, LV_STATE_DEFAULT);
    lv_obj_set_size(top_line, 40, 2);
    lv_obj_align(top_line, LV_ALIGN_TOP_MID, 0, 10);

    /* 图标 (wifi 卡复用 200x200 大图, 缩到 40px) */
    lv_obj_t* icon = img_create(card, icon_path);
    lv_coord_t src_w = lv_image_get_src_width(icon);
    if (src_w > 0)
        lv_image_set_scale(icon, 256 * 40 / src_w);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 24);
    if (wifi_card)
        g_wifi_icon = icon;

    /* 标题 + 状态值 */
    lv_obj_t* t = label_create(card, 14, lv_color_hex(0xE6ECFF));
    lv_label_set_text(t, title);
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -32);

    lv_obj_t* st = label_create(card, 12, lv_color_hex(0x9DB4FF));
    lv_label_set_text(st, state);
    lv_obj_align(st, LV_ALIGN_BOTTOM_MID, 0, -10);
    if (wifi_card)
        g_wifi_state_label = st;
    else if (strcmp(title, "亮度") == 0)
        g_bright_state_label = st;

    return card;
}

/* 设置卡: 点击 → 设置页 */
static void setting_card_click_cb(lv_event_t* e) {
    setting_screen_show();
}

/* ==================== 主面板 ==================== */
static lv_obj_t* main_panel_create(void) {
    lv_obj_t* panel = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &style_main_panel, LV_STATE_DEFAULT);
    lv_obj_set_size(panel, 1320, 232);
    lv_obj_set_pos(panel, 52, 18);

    time_area_create(panel);
    weather_area_create(panel);
    divider_create(panel);

    /* 4 张功能卡片 (卡 100 宽间距 10, 卡组 panel 内 798..1228, 箭头 1244 贴卡组右侧) */
    feature_card_create(panel, 798, IMAGE_PATH "iconfont_wifi.png", "WIFI", "未连接", true);
    feature_card_create(panel, 908, IMAGE_PATH "app_music.png", "音乐", "运行中", false);
    feature_card_create(panel, 1018, IMAGE_PATH "brightness.png", "亮度", "--", false);
    lv_obj_t* set_card = feature_card_create(panel, 1128, IMAGE_PATH "app_setting.png", "设置", "点击进入", false);
    lv_obj_add_event_cb(set_card, setting_card_click_cb, LV_EVENT_CLICKED, NULL);

    /* > 箭头 (暗示可左右滑动) */
    lv_obj_t* arrow = img_create(panel, IMAGE_PATH "right_arrow.png");
    lv_obj_align(arrow, LV_ALIGN_TOP_LEFT, 1244, 94);

    return panel;
}

/* ==================== 底部: 波浪缺口 + 提示条 ==================== */
static void bottom_tip_create(void) {
    /* 波浪缺口图: 盖在面板底缘, 挖出 3 个半圆缺口 (露出背景) */
    lv_obj_t* wave = img_create(lv_screen_active(), IMAGE_PATH "wave.png");
    lv_obj_set_pos(wave, 462, 220);

    /* 提示条: 横跨中间, 半透明胶囊探出面板底缘 */
    lv_obj_t* bar = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(bar);
    lv_obj_add_style(bar, &style_tip_bar, LV_STATE_DEFAULT);
    lv_obj_set_size(bar, 304, 40);
    lv_obj_set_pos(bar, 560, 228);

    g_tip_label = label_create(bar, 14, lv_color_hex(0xE6ECFF));
    lv_label_set_text(g_tip_label, "夜间出行请注意添衣");
    lv_obj_center(g_tip_label);
}

/* ==================== 定时刷新 ==================== */

/* 1s: 大时钟 + 日期行 + 提示条文案 (按时间段) */
static void main_clock_cb(lv_timer_t* t) {
    (void)t;
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    if (tm == NULL)
        return;

    char buf[80];
    snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
    if (g_clock_label)
        lv_label_set_text(g_clock_label, buf);

    /* 跨天才重算日期行 (时钟每秒刷新, 字符串拼接能省则省) */
    static int last_day = -1;
    if (tm->tm_mday != last_day) {
        last_day = tm->tm_mday;
        int ly, lm, ld, is_leap;
        solar_to_lunar(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                       &ly, &lm, &ld, &is_leap);
        snprintf(buf, sizeof(buf), "%s %04d-%02d-%02d %s%s年%s%s月%s",
                 s_week_cn[tm->tm_wday],
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 s_gan[(ly - 4) % 10], s_zhi[(ly - 4) % 12],
                 is_leap ? "闰" : "", s_lunar_month_cn[lm - 1], s_lunar_day_cn[ld - 1]);
        if (g_date_label)
            lv_label_set_text(g_date_label, buf);
    }

    /* 提示条按时间段切换 */
    static int last_hour = -1;
    if (tm->tm_hour != last_hour) {
        last_hour = tm->tm_hour;
        const char* tip = (tm->tm_hour >= 6 && tm->tm_hour < 18)
                              ? "今日天气晴好 适宜户外活动"
                              : "夜间出行请注意添衣";
        if (g_tip_label)
            lv_label_set_text(g_tip_label, tip);
    }
}

/* 1s: WIFI 卡状态 */
static void wifi_card_cb(lv_timer_t* t) {
    (void)t;
    if (g_wifi_state_label == NULL)
        return;
    lv_label_set_text(g_wifi_state_label, wifi_state_text());
    if (g_wifi_icon) {
        lv_image_set_src(g_wifi_icon,
                         g_conn_status == WPA_WIFI_CONNECT
                             ? IMAGE_PATH "iconfont_wifi.png"
                             : IMAGE_PATH "iconfont_wifi_off.png");
    }
}

/* 10s: 室内温湿度 (模拟微动) + 背光亮度 */
static void sensor_cb(lv_timer_t* t) {
    (void)t;
    static int temp_x = 265; /* 26.5°C (存 10 倍) */
    static int humi_x = 58;  /* 58% */

    time_t now = time(NULL);
    /* 板上无温湿度传感器: 以时间为种子做 ±0.2°C / ±1% 伪随机微动 (接入真实传感器后替换) */
    temp_x += (int)(now % 5) - 2;
    humi_x += (int)(now % 5) - 2;
    if (temp_x < 240) temp_x = 240;
    if (temp_x > 290) temp_x = 290;
    if (humi_x < 50) humi_x = 50;
    if (humi_x > 65) humi_x = 65;

    char buf[24];
    if (g_temp_label) {
        snprintf(buf, sizeof(buf), "%d.%d°C", temp_x / 10, temp_x % 10);
        lv_label_set_text(g_temp_label, buf);
    }
    if (g_humi_label) {
        snprintf(buf, sizeof(buf), "%d%%", humi_x);
        lv_label_set_text(g_humi_label, buf);
    }
    if (g_bright_state_label) {
        int pct = read_brightness_pct();
        if (pct < 0)
            pct = 72; /* 模拟器/读取失败: 默认 72% */
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(g_bright_state_label, buf);
    }
}

/* ==================== 入口 ==================== */
void set_init(void) {
    main_panel_style_init();

    /* 主页面建在默认 screen 上, 保存句柄供设置页"返回"切换 */
    g_home_screen = lv_screen_active();

    /* 背景 (demo6 同款) */
    img_create(g_home_screen, IMAGE_PATH "back.png");

    main_panel_create();
    bottom_tip_create();

    /* 立即刷新一次, 之后定时器周期刷新 */
    main_clock_cb(NULL);
    wifi_card_cb(NULL);
    sensor_cb(NULL);

    lv_timer_create(main_clock_cb, 1000, NULL);
    lv_timer_create(wifi_card_cb, 1000, NULL);
    lv_timer_create(sensor_cb, 10000, NULL);

    /* 设置页: 独立 screen (页面对象全部挂它, 与主页面互不影响) */
    setting_init();
}
