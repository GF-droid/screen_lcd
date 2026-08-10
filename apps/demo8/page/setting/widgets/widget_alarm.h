#ifndef _WIDGET_ALARM_H_
#define _WIDGET_ALARM_H_

#include "page_conf.h"

/* 开关胶囊/分隔线等样式, 被 dev/sys 组件复用 */
void alarm_manage_style_init(void);
/* 组件入口 (设置页卡片内创建) */
lv_obj_t* alarm_manage_create(lv_obj_t* parent);

#endif  // _WIDGET_ALARM_H_
