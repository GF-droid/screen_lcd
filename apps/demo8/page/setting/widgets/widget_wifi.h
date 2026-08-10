#ifndef _WIDGET_WIFI_H_
#define _WIDGET_WIFI_H_

#include "page_conf.h"

/* WiFi 连接条组件类型 */
typedef struct {
    lv_obj_t* bar;          /* 整条容器 */
    lv_obj_t* ssid_input;   /* 账号输入框 */
    lv_obj_t* pwd_input;    /* 密码输入框 */
    lv_obj_t* connect_btn;  /* 连接按钮 */
    lv_obj_t* loading_img;  /* 加载动画图片 */
    lv_obj_t* success_img;  /* 连接成功图片 */
    lv_obj_t* hint_label;   /* 提示文字 (红) */
} wifi_connect_bar_t;

void wifi_connect_bar_style_init(void);
wifi_connect_bar_t wifi_connect_bar_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);
/* 组件入口 (设置页卡片内创建) */
lv_obj_t* wifi_component_create(lv_obj_t* parent);
/* 切换组件时收起屏幕键盘 (setting.c 调用) */
void wifi_kb_hide(void);

#endif  // _WIDGET_WIFI_H_
