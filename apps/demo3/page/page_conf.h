#ifndef _PAGE_CONF_H_
#define _PAGE_CONF_H_

#include <stdio.h>
#include "lv_conf.h"
#include "lvgl.h"
#include "res/font/font_conf.h"
#include "wpa_manager.h"

/* 设置行组件类型 */
typedef struct {
    lv_obj_t* row;         /* 行容器 (flex row) */
    lv_obj_t* icon_slot;   /* 左侧图标槽 */
    lv_obj_t* label_left;  /* 左侧文字 */
    lv_obj_t* label_right; /* 右侧第二列文字 */
} settings_row_t;

void set_init(void);
/* 状态卡片组件: 返回卡片本体, 直接往里面加内容即可 */
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

/* 闹钟管理组件: 左半闹钟设置(时/分 roller + 开关), 右半倒计时(分/秒 roller + 开始/暂停/重置) */
void alarm_manage_style_init(void);
lv_obj_t* alarm_manage_create(lv_obj_t* parent);
/* 程序化底部栏 (替换 bottom.png): 全宽半透明渐变横条, 返回容器对象 */
lv_obj_t* bottom_bar_create(lv_obj_t* parent);
/* 底部栏配套按钮 (深蓝玻璃胶囊, 与底部栏同色系) */
lv_obj_t* bottom_bar_btn_create(lv_obj_t* bar, const char* text);
/* wpa_manager 事件线程回调 → UI 桥 (只写标志位, UI 更新在主循环轮询里做, 线程安全) */
void wifi_status_ui_cb(WPA_WIFI_CONNECT_STATUS_E status);
/* 闹钟管理样式 (定义在 page_component.c, 供 page_set.c 的 create 使用) */
extern lv_style_t style_alarm_roller;
extern lv_style_t style_alarm_roller_sel;
extern lv_style_t style_alarm_switch;
extern lv_style_t style_alarm_switch_checked;
extern lv_style_t style_alarm_small_btn;
extern lv_style_t style_alarm_small_btn_pressed;
extern lv_style_t style_alarm_small_btn_disabled;
extern lv_style_t style_alarm_divider;

#endif  // _PAGE_CONF_H_
