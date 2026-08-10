#ifndef _SETTING_H_
#define _SETTING_H_

#include "page_conf.h"

void setting_init(void); /* 创建设置页 screen (挂在 g_setting_screen 上) */

/* 顶栏音量显示: 供 widget_dev.c 联动刷新 (顶栏元素是 setting.c 内部 static, 不暴露对象) */
void setting_top_vol_set(int pct);

/* 顶栏 WiFi 图标/文案刷新 (各组件定时器轮询调用, 返回状态是否变化) */
bool update_top_wifi_ui(void);

#endif  // _SETTING_H_
