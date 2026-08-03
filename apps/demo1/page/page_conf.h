#ifndef _PAGE_CONF_H_
#define _PAGE_CONF_H_

#include <stdio.h>
#include "lv_conf.h"
#include "lvgl.h"
#include "res/font/font_conf.h"

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

#endif  // _PAGE_CONF_H_
