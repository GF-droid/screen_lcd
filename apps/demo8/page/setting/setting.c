#include "setting.h"
#include "widgets/widget_common.h"
#include "widgets/widget_wifi.h"
#include "widgets/widget_netinfo.h"
#include "widgets/widget_alarm.h"
#include "widgets/widget_dev.h"
#include "widgets/widget_sysinfo.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


/* ============ 公共: 顶栏动态元素 ============ */
/* 设置页独立 screen 句柄 (由 main 侧创建, 跳转用 lv_screen_load) */
lv_obj_t* g_setting_screen = NULL;
static lv_obj_t* g_top_time_label = NULL;
static lv_obj_t* g_top_wifi_icon = NULL;
static lv_obj_t* g_top_wifi_label = NULL;
static lv_obj_t* g_top_vol_label = NULL; /* 音量: 右栏 slider 变化时联动刷新 */
/* 上次顶栏 WiFi 状态 (状态没变不刷新). 初值 true: 顶栏创建时写死"已连接",
 * 真实状态未连接时首次轮询才会切到白色图标, 避免启动瞬间误跳 */
static bool g_top_wifi_connected = true;

/* wpa_manager 事件线程写, 主循环轮询读 (只写标志位, 线程安全) */
volatile WPA_WIFI_CONNECT_STATUS_E g_conn_status = WPA_WIFI_INACTIVE; /* 主页+设置页共享 (page_conf.h extern) */

static void setting_back_btn_cb(lv_event_t* e); /* 前置声明 (定义在 setting_init 之后) */
/* 顶栏时钟回调 (定义在 top_component_add_info 之后) */
static void top_clock_timer_cb(lv_timer_t* t);

// 封装一个图标显示函数
static lv_obj_t* lv_image_vewer_create(lv_obj_t* parent, const char* image_path, lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, image_path);
    lv_obj_align(img, align, x_ofs, y_ofs);
    return img;
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
        } else if (strncmp(items[i].text, "音量:", 6) == 0) {
            g_top_vol_label = label; /* 音量: 右栏 slider 变化时联动刷新 */
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

/* 顶栏 WiFi 图标/文案: 连接成功=蓝色图标, 未连接=白色图标 (状态没变不刷新)
 * 返回状态是否发生变化, 变化时由 sys_timer_cb 记录日志 */
bool update_top_wifi_ui(void) {
    if (g_top_wifi_icon == NULL)
        return false;
    bool conn = (g_conn_status == WPA_WIFI_CONNECT);
    if (conn == g_top_wifi_connected)
        return false;
    g_top_wifi_connected = conn;
    lv_image_set_src(g_top_wifi_icon,
                     conn ? IMAGE_PATH "iconfont_wifi.png"       /* 蓝: 已连接 */
                          : IMAGE_PATH "iconfont_wifi_off.png"); /* 白: 未连接 */
    lv_label_set_text(g_top_wifi_label, conn ? "WIFI:已连接" : "WIFI:未连接");
    return true;
}

/* wpa_manager 事件线程回调 → 只写标志位, 绝不操作 LVGL (非线程安全) */
void wifi_status_ui_cb(WPA_WIFI_CONNECT_STATUS_E status) {
    g_conn_status = status;
}


/* ==================== 组件切换: 左侧行点击 → 右侧卡片换内容 ==================== */
typedef enum {
    COMP_WIFI = 0,  /* 网络连接 (demo2 wifi连接条) */
    COMP_NETINFO,   /* 网络设置 (demo1 气泡网络信息) */
    COMP_ALARM,     /* 闹钟管理 */
    COMP_BT,        /* 蓝牙设置 */
    COMP_SYS,       /* 日志记录 / 系统更新 (默认) */
    COMP_MAX
} comp_id_t;

/* 4 个功能组件在卡片内预创建, 切换 = 互斥 hide/show (保留各组件内部状态, 无重建延迟) */
static lv_obj_t* g_comps[COMP_MAX] = {NULL};
static comp_id_t g_cur_comp = COMP_SYS;

static void switch_component(comp_id_t id) {
    if (id == g_cur_comp)
        return;
    /* 离开 WiFi 页时收起屏幕键盘 (键盘建在 screen 上, 不属于任何组件) */
    if (g_cur_comp == COMP_WIFI)
        wifi_kb_hide(); /* 离开 WiFi 页时收起屏幕键盘 (键盘对象在 widget_wifi.c 内部) */
    if (g_comps[id] != NULL)
        lv_obj_clear_flag(g_comps[id], LV_OBJ_FLAG_HIDDEN);
    if (g_comps[g_cur_comp] != NULL)
        lv_obj_add_flag(g_comps[g_cur_comp], LV_OBJ_FLAG_HIDDEN);
    g_cur_comp = id;
}

/* 行点击回调: user_data = 目标组件 id */
static void row_switch_cb(lv_event_t* e) {
    switch_component((comp_id_t)(intptr_t)lv_event_get_user_data(e));
}

/* 注册中间矩形组件: 状态卡片 + 预创建全部 4 个功能组件 (默认只显示系统信息) */
static lv_obj_t* lv_mid_screen_componnet_create(lv_obj_t* parent) {
    /* 状态卡片: 靠屏幕右半区 (卡片 60% 宽, 从右侧向内收, 不与左侧设置列表重叠) */
    lv_obj_t* sp_panel = status_panel_create(parent, LV_PCT(60), LV_PCT(60));
    lv_obj_align(sp_panel, LV_ALIGN_RIGHT_MID, -20, 0);

    /* 全部预创建: 各自内部状态 (输入内容/倒计时/滑杆值) 在切换间保留,
       每个组件自带的定时器 (连接轮询/闹钟比对/采样) 在后台持续运行 */
    g_comps[COMP_WIFI] = wifi_component_create(sp_panel);
    g_comps[COMP_NETINFO] = net_info_component_create(sp_panel);
    g_comps[COMP_ALARM] = alarm_manage_create(sp_panel);
    g_comps[COMP_BT] = bt_setting_create(sp_panel);
    g_comps[COMP_SYS] = sys_update_create(sp_panel);

    /* 默认显示系统信息, 其余隐藏 */
    g_cur_comp = COMP_SYS;
    for (int i = 0; i < COMP_MAX; i++) {
        if (g_comps[i] != NULL && i != g_cur_comp)
            lv_obj_add_flag(g_comps[i], LV_OBJ_FLAG_HIDDEN);
    }

    return sp_panel;
}

void setting_init(void) {
    font_init(); /* 注册 TTF 字体路径, 必须在任何 get_font() 之前 */

    /* 独立 screen: 设置页全部对象挂这里, 与主页面 (默认 screen) 互不影响,
     * 主页↔设置页用 lv_screen_load 切换 */
    g_setting_screen = lv_obj_create(NULL); /* v9: NULL parent 即 screen */

    lv_image_vewer_create(g_setting_screen, IMAGE_PATH "back.png", LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t* top_box = top_component_create(g_setting_screen); /* 第一部分: 盒子 */
    top_component_add_info(top_box);                              /* 第二部分: 显示信息 */

    lv_obj_t* bottom_img = bottom_bar_create(g_setting_screen); /* 程序化底部栏, 替换 bottom.png */

    lv_obj_t* panel = settings_panel_create(g_setting_screen, LV_PCT(20), LV_PCT(65));
    lv_obj_align_to(panel, g_setting_screen, LV_ALIGN_LEFT_MID, 107, 0);
    lv_obj_update_layout(panel);

    lv_font_t* font = get_font(FONT_TYPE_CN, 10);
    if (font != NULL) {
        lv_obj_set_style_text_font(panel, font, LV_STATE_DEFAULT);
    }
    settings_row_t row_bt = row_add_clickable(panel, IMAGE_PATH "iconfont_bule.png", "蓝牙设置", "显示与亮度");
    settings_row_t row_disp = row_add_clickable(panel, IMAGE_PATH "wangl.png", "网络连接 ", "wifi连接");
    settings_row_t row_update = row_add_clickable(panel, IMAGE_PATH "iconfont_genx.png", "网络设置", "蓝牙设置");
    settings_row_t row_alarm = row_add_clickable(panel, IMAGE_PATH "iconfont_laoz.png", "闹钟管理", "闹钟管理");
    settings_row_t row_log = row_add_clickable(panel, IMAGE_PATH "iconfont_riz.png", "日志记录", "系统更新");

    settings_row_set_selected(&row_log, true); /* 初始选中"日志记录/系统更新"行 (默认组件也是它) */

    /* 行点击 → 右侧卡片切换 (互斥选中由 row_click_cb 负责, 这里是内容切换;
       第 2、3 行都指向 WiFi 组件) */
    lv_obj_add_event_cb(row_bt.row, row_switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)COMP_BT);
    lv_obj_add_event_cb(row_disp.row, row_switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)COMP_WIFI);
    lv_obj_add_event_cb(row_update.row, row_switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)COMP_NETINFO);
    lv_obj_add_event_cb(row_alarm.row, row_switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)COMP_ALARM);
    lv_obj_add_event_cb(row_log.row, row_switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)COMP_SYS);

    lv_obj_t* sp_panel = lv_mid_screen_componnet_create(g_setting_screen);
    lv_obj_align_to(sp_panel, panel, LV_ALIGN_OUT_RIGHT_MID, 66, 0);

    lv_obj_t* back_btn = bottom_bar_btn_create(bottom_img, "返回"); /* 深蓝玻璃, 与底部栏同色系 */
    lv_obj_add_event_cb(back_btn, setting_back_btn_cb, LV_EVENT_CLICKED, NULL); /* 返回 → 主页面 */

    /* 字体设在按钮对象上, 经继承链传给内部文案 label */
    lv_font_t* font_btn = get_font(FONT_TYPE_CN, 30);
    if (font_btn != NULL) {
        lv_obj_set_style_text_font(back_btn, font_btn, LV_STATE_DEFAULT);
    }
}

/* 设置页"返回" → 切回主页面 (screen 保留, lv_screen_load 只是换 active) */
static void setting_back_btn_cb(lv_event_t* e) {
    home_screen_show();
}

/* 顶栏音量显示: 供 widget_dev.c 联动刷新 (顶栏元素是本文件内部 static, 只暴露 setter) */
void setting_top_vol_set(int pct) {
    if (g_top_vol_label == NULL)
        return;
    char buf[24];
    snprintf(buf, sizeof(buf), "音量:%d%%", pct);
    lv_label_set_text(g_top_vol_label, buf);
}
