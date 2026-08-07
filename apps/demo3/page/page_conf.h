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

/* WiFi 连接条组件: 账号输入 + 密码输入 + 连接按钮 + 加载/成功图标, 整条 flex 横向排布 */
typedef struct {
    lv_obj_t* bar;          /* 整条背景 (flex row) */
    lv_obj_t* ssid_input;   /* WiFi 账号输入框 */
    lv_obj_t* pwd_input;    /* 密码输入框 (遮挡显示) */
    lv_obj_t* connect_btn;  /* 连接按钮 */
    lv_obj_t* loading_img;  /* 加载动画 (帧序列旋转, 连接中显示) */
    lv_obj_t* success_img;  /* 连接成功图标 (对勾, 成功后显示) */
    lv_obj_t* hint_label;   /* 提示文字 (密码错误/连接超时等, 失败时显示) */
} wifi_connect_bar_t;

void wifi_connect_bar_style_init(void);
wifi_connect_bar_t wifi_connect_bar_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);
/* 程序化底部栏 (替换 bottom.png): 全宽半透明渐变横条, 返回容器对象 */
lv_obj_t* bottom_bar_create(lv_obj_t* parent);
/* 底部栏配套按钮 (深蓝玻璃胶囊, 与底部栏同色系) */
lv_obj_t* bottom_bar_btn_create(lv_obj_t* bar, const char* text);
/* wpa_manager 事件线程回调 → UI 桥 (只写标志位, UI 更新在主循环轮询里做, 线程安全) */
void wifi_status_ui_cb(WPA_WIFI_CONNECT_STATUS_E status);
/* WiFi 连接条的样式 (定义在 page_component.c, 供 page_set.c 的 create 使用) */
extern lv_style_t style_bar;
extern lv_style_t style_input;
extern lv_style_t style_btn;
extern lv_style_t style_btn_pressed;
extern lv_style_t style_btn_label;

#endif  // _PAGE_CONF_H_
