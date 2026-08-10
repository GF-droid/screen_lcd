#ifndef _WIDGET_SYSINFO_H_
#define _WIDGET_SYSINFO_H_

#include "page_conf.h"

void sys_style_init(void);
/* 组件入口 (设置页卡片内创建) */
lv_obj_t* sys_update_create(lv_obj_t* parent);

#endif  // _WIDGET_SYSINFO_H_
