#ifndef _PAGE_CONF_H_
#define _PAGE_CONF_H_

#include <stdio.h>
#include "lv_conf.h"
#include "lvgl.h"
#include "res/font/font_conf.h"
#include "res/music/music_conf.h"
#include "wpa_manager.h"

/* ============ 页面级接口 (仅页面切换与跨页面回调) ============ */

/* 页面切换: 主页/设置页各占一个独立 screen, lv_screen_load 互切 (screen 对象保留不销毁) */
void set_init(void);               /* 主页面初始化 (home.c) */
void setting_init(void);           /* 设置页初始化 (setting.c) */
void setting_screen_show(void);    /* 主页面 → 设置页 (home.c) */
void home_screen_show(void);       /* 设置页 → 主页面 (home.c) */
extern lv_obj_t* g_home_screen;
extern lv_obj_t* g_setting_screen;

/* wpa_manager 事件线程回调 → UI 桥 (定义在 setting.c, 只写标志位, UI 更新在主循环轮询, 线程安全) */
void wifi_status_ui_cb(WPA_WIFI_CONNECT_STATUS_E status);
extern volatile WPA_WIFI_CONNECT_STATUS_E g_conn_status;

#endif  // _PAGE_CONF_H_
