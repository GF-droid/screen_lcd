#ifndef _WIDGET_DEV_H_
#define _WIDGET_DEV_H_

#include "page_conf.h"

void bt_slider_style_init(void);
/* 组件入口 (设置页卡片内创建) */
lv_obj_t* bt_setting_create(lv_obj_t* parent);

#endif  // _WIDGET_DEV_H_
