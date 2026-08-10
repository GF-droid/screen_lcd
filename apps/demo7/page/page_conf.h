#ifndef _PAGE_CONF_H_
#define _PAGE_CONF_H_

#include <stdio.h>
#include "lv_conf.h"
#include "lvgl.h"
#include "res/font/font_conf.h"
#include "res/music/music_conf.h"
#include "wpa_manager.h"

/* ================= 主页面 (page_set.c + page_component.c) ================= */

void set_init(void);
void main_panel_style_init(void);

/* 通用样式 (定义在 page_component.c, 供 page_set.c 的 create 使用) */
extern lv_style_t style_main_panel;
extern lv_style_t style_weather_card;
extern lv_style_t style_feature_card;
extern lv_style_t style_tip_bar;
extern lv_style_t style_glow_line;
extern lv_style_t style_glow_dot;

/* ================= 设置页 (page_setting.c + page_setting_component.c, 移植自 demo6) ================= */

/* 设置行组件类型 */
typedef struct {
    lv_obj_t* row;         /* 行容器 (flex row) */
    lv_obj_t* icon_slot;   /* 左侧图标槽 */
    lv_obj_t* label_left;  /* 左侧文字 */
    lv_obj_t* label_right; /* 右侧第二列文字 */
} settings_row_t;

/* WiFi 连接条组件类型 (demo2 移植) */
typedef struct {
    lv_obj_t* bar;          /* 整条容器 */
    lv_obj_t* ssid_input;   /* 账号输入框 */
    lv_obj_t* pwd_input;    /* 密码输入框 */
    lv_obj_t* connect_btn;  /* 连接按钮 */
    lv_obj_t* loading_img;  /* 加载动画图片 */
    lv_obj_t* success_img;  /* 连接成功图片 */
    lv_obj_t* hint_label;   /* 提示文字 (红) */
} wifi_connect_bar_t;

/* 页面切换: 主页/设置页各占一个独立 screen, lv_screen_load 互切 (screen 对象保留不销毁) */
void setting_init(void);      /* 创建设置页 screen (挂在 g_setting_screen 上) */
void setting_screen_show(void); /* 主页面 → 设置页 */
void home_screen_show(void);    /* 设置页 → 主页面 */
extern lv_obj_t* g_setting_screen;
extern lv_obj_t* g_home_screen;

/* wpa_manager 事件线程回调 → UI 桥 (定义在 page_setting.c, 只写标志位, UI 更新在主循环轮询, 线程安全) */
void wifi_status_ui_cb(WPA_WIFI_CONNECT_STATUS_E status);
extern volatile WPA_WIFI_CONNECT_STATUS_E g_conn_status;

/* ---------- 设置页组件 (定义在 page_setting_component.c) ---------- */
lv_obj_t* status_panel_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);
void glass_glow_btn_style_init(void);
lv_obj_t* glass_glow_btn_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, const char* text);
lv_obj_t* settings_panel_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);
settings_row_t settings_row_add(lv_obj_t* panel, const char* icon_path, const char* left_text, const char* right_text);
void settings_row_set_selected(settings_row_t* row, bool selected);
/* 创建一行并注册点击选中事件 */
settings_row_t row_add_clickable(lv_obj_t* panel, const char* icon_path, const char* left_text, const char* right_text);

/* 气泡胶囊组件 (玻璃质感, 内部 [图标槽] [文案]) */
lv_obj_t* chip_bubble_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, const char* text);

/* 闹钟管理组件 */
void alarm_manage_style_init(void);
lv_obj_t* alarm_manage_create(lv_obj_t* parent);
/* 蓝牙/亮度/音量组件 */
void bt_slider_style_init(void);
lv_obj_t* bt_setting_create(lv_obj_t* parent);
/* 日志记录 + 系统更新组件: 左栏系统信息/版本, 右栏运行状态图表 + 日志底条 */
void sys_style_init(void);
lv_obj_t* sys_update_create(lv_obj_t* parent);
/* WiFi 连接条组件 */
void wifi_connect_bar_style_init(void);
wifi_connect_bar_t wifi_connect_bar_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);
/* 程序化底部栏 (替换 bottom.png): 全宽半透明渐变横条, 返回容器对象 */
lv_obj_t* bottom_bar_create(lv_obj_t* parent);
/* 底部栏配套按钮 (深蓝玻璃胶囊, 与底部栏同色系) */
lv_obj_t* bottom_bar_btn_create(lv_obj_t* bar, const char* text);

/* 通用样式 (定义在 page_setting_component.c, 供 page_setting.c 的 create 使用) */
extern lv_style_t style_alarm_roller;
extern lv_style_t style_alarm_roller_sel;
extern lv_style_t style_alarm_switch;
extern lv_style_t style_alarm_switch_checked;
extern lv_style_t style_alarm_small_btn;
extern lv_style_t style_alarm_small_btn_pressed;
extern lv_style_t style_alarm_small_btn_disabled;
extern lv_style_t style_alarm_divider;
extern lv_style_t style_sys_log_bg;
extern lv_style_t style_bar;
extern lv_style_t style_input;
extern lv_style_t style_btn;
extern lv_style_t style_btn_pressed;
extern lv_style_t style_btn_label;
extern lv_style_t style_bt_slider_main;
extern lv_style_t style_bt_slider_ind;
extern lv_style_t style_bt_slider_knob;

#endif  // _PAGE_CONF_H_
