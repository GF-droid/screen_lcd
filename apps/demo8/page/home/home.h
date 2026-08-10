#ifndef _HOME_H_
#define _HOME_H_

#include "page_conf.h"

/* ---------- 主页样式 (home_style.c), 供 home.c 的 create 使用 ---------- */
void main_panel_style_init(void);
extern lv_style_t style_main_panel;
extern lv_style_t style_weather_card;
extern lv_style_t style_feature_card;
extern lv_style_t style_tip_bar;
extern lv_style_t style_glow_line;
extern lv_style_t style_glow_dot;

/* ---------- 公历 → 农历 (home_lunar.c) ---------- */
void solar_to_lunar(int sy, int sm, int sd, int* ly, int* lm, int* ld, int* is_leap);

#endif  // _HOME_H_
