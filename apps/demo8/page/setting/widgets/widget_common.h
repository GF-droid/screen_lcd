#ifndef _WIDGET_COMMON_H_
#define _WIDGET_COMMON_H_

#include "page_conf.h"

/* 设置行组件类型 */
typedef struct {
    lv_obj_t* row;         /* 行容器 (flex row) */
    lv_obj_t* icon_slot;   /* 左侧图标槽 */
    lv_obj_t* label_left;  /* 左侧文字 */
    lv_obj_t* label_right; /* 右侧第二列文字 */
} settings_row_t;

/* ---------- 通用控件工厂 ---------- */
void glass_glow_btn_style_init(void);
lv_obj_t* glass_glow_btn_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, const char* text);
lv_obj_t* settings_panel_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);
settings_row_t settings_row_add(lv_obj_t* panel, const char* icon_path, const char* left_text, const char* right_text);
/* 创建一行并注册点击选中事件 */
settings_row_t row_add_clickable(lv_obj_t* panel, const char* icon_path, const char* left_text, const char* right_text);
void settings_row_set_selected(settings_row_t* row, bool selected);
lv_obj_t* status_panel_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);
/* 气泡胶囊组件 (玻璃质感, 内部 [图标槽] [文案]) */
lv_obj_t* chip_bubble_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, const char* text);
/* 程序化底部栏 (替换 bottom.png): 全宽半透明渐变横条, 返回容器对象 */
lv_obj_t* bottom_bar_create(lv_obj_t* parent);
/* 底部栏配套按钮 (深蓝玻璃胶囊, 与底部栏同色系) */
lv_obj_t* bottom_bar_btn_create(lv_obj_t* bar, const char* text);

/* ---------- 通用样式 (跨组件复用) ---------- */
extern lv_style_t style_alarm_roller;
extern lv_style_t style_alarm_roller_sel;
extern lv_style_t style_alarm_switch;
extern lv_style_t style_alarm_switch_checked;
extern lv_style_t style_alarm_small_btn;
extern lv_style_t style_alarm_small_btn_pressed;
extern lv_style_t style_alarm_small_btn_disabled;
extern lv_style_t style_alarm_divider;
extern lv_style_t style_bar;
extern lv_style_t style_input;
extern lv_style_t style_btn;
extern lv_style_t style_btn_pressed;
extern lv_style_t style_btn_label;

#endif  // _WIDGET_COMMON_H_
