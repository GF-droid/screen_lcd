#include "home.h"

#include "page_conf.h"

/*
 * demo7 主页面组件样式:
 * 一条超宽横向玻璃面板(四区块 + 波浪提示条), 风格延续 demo6
 * (深色渐变 + 发光描边 + 圆角)。样式全部非 static, page_set.c 的 create 用。
 */

lv_style_t style_main_panel;   /* 超宽主面板: 不透明渐变底 + 发光描边 + 大圆角 */
lv_style_t style_weather_card; /* 天气卡片: 半透明深色 + 青绿描边 + 青绿辉光 */
lv_style_t style_feature_card; /* 功能卡片: 半透明 + 淡蓝描边 + 微辉光 */
lv_style_t style_tip_bar;      /* 底部提示条: 全胶囊半透明 + 细发光描边 */
lv_style_t style_glow_line;    /* 发光短线/竖线: 实色底 + 光晕 (尺寸由对象定) */
lv_style_t style_glow_dot;     /* 分隔线中点装饰: 8x8 发光方点 */
static bool main_style_inited = false;

void main_panel_style_init(void) {
    if (main_style_inited)
        return;
    main_style_inited = true;

    /* ---------- 主面板: 发光描边包裹的不透明渐变 ---------- */
    lv_style_init(&style_main_panel);
    lv_style_set_radius(&style_main_panel, 32);
    lv_style_set_bg_opa(&style_main_panel, LV_OPA_COVER);
    lv_style_set_bg_color(&style_main_panel, lv_color_hex(0x2B2F72));      /* 面板-顶部 */
    lv_style_set_bg_grad_color(&style_main_panel, lv_color_hex(0x11142E)); /* 面板-底部 */
    lv_style_set_bg_grad_dir(&style_main_panel, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_main_panel, 1);
    lv_style_set_border_color(&style_main_panel, lv_color_hex(0x9DB4FF));
    lv_style_set_border_opa(&style_main_panel, LV_OPA_40);
    lv_style_set_shadow_width(&style_main_panel, 28);
    lv_style_set_shadow_spread(&style_main_panel, 3);
    lv_style_set_shadow_color(&style_main_panel, lv_color_hex(0x7C6CFF));
    lv_style_set_shadow_opa(&style_main_panel, LV_OPA_30);
    lv_style_set_clip_corner(&style_main_panel, true); /* 波浪图盖在底缘也不会露出圆角外 */
    lv_style_set_pad_all(&style_main_panel, 0);

    /* ---------- 天气卡片: 独立圆角卡片, 青绿系发光 ---------- */
    lv_style_init(&style_weather_card);
    lv_style_set_radius(&style_weather_card, 22);
    lv_style_set_bg_opa(&style_weather_card, 75);
    lv_style_set_bg_color(&style_weather_card, lv_color_hex(0x1A1F4A));      /* 顶部 */
    lv_style_set_bg_grad_color(&style_weather_card, lv_color_hex(0x12162F)); /* 底部 */
    lv_style_set_bg_grad_dir(&style_weather_card, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_weather_card, 1);
    lv_style_set_border_color(&style_weather_card, lv_color_hex(0x3CE0C8));
    lv_style_set_border_opa(&style_weather_card, LV_OPA_40);
    lv_style_set_shadow_width(&style_weather_card, 18);
    lv_style_set_shadow_color(&style_weather_card, lv_color_hex(0x3CE0C8));
    lv_style_set_shadow_opa(&style_weather_card, 25);
    lv_style_set_pad_all(&style_weather_card, 0);

    /* ---------- 功能卡片: 半透明深蓝玻璃 + 淡蓝描边 ---------- */
    lv_style_init(&style_feature_card);
    lv_style_set_radius(&style_feature_card, 18);
    lv_style_set_bg_opa(&style_feature_card, 55);
    lv_style_set_bg_color(&style_feature_card, lv_color_hex(0x232963));
    lv_style_set_border_width(&style_feature_card, 1);
    lv_style_set_border_color(&style_feature_card, lv_color_hex(0x9DB4FF));
    lv_style_set_border_opa(&style_feature_card, 25);
    lv_style_set_shadow_width(&style_feature_card, 12);
    lv_style_set_shadow_color(&style_feature_card, lv_color_hex(0x7C6CFF));
    lv_style_set_shadow_opa(&style_feature_card, LV_OPA_20);
    lv_style_set_pad_all(&style_feature_card, 0);

    /* ---------- 底部提示条: 全胶囊 + 细发光描边 ---------- */
    lv_style_init(&style_tip_bar);
    lv_style_set_radius(&style_tip_bar, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_tip_bar, 92);
    lv_style_set_bg_color(&style_tip_bar, lv_color_hex(0x11142E));
    lv_style_set_border_width(&style_tip_bar, 1);
    lv_style_set_border_color(&style_tip_bar, lv_color_hex(0x9DB4FF));
    lv_style_set_border_opa(&style_tip_bar, LV_OPA_30);
    lv_style_set_shadow_width(&style_tip_bar, 10);
    lv_style_set_shadow_color(&style_tip_bar, lv_color_hex(0x7C6CFF));
    lv_style_set_shadow_opa(&style_tip_bar, 25);
    lv_style_set_pad_all(&style_tip_bar, 0);

    /* ---------- 发光短线/竖线: 尺寸由具体对象设置 ---------- */
    lv_style_init(&style_glow_line);
    lv_style_set_bg_opa(&style_glow_line, LV_OPA_COVER);
    lv_style_set_bg_color(&style_glow_line, lv_color_hex(0x9DB4FF));
    lv_style_set_shadow_width(&style_glow_line, 8);
    lv_style_set_shadow_color(&style_glow_line, lv_color_hex(0x9DB4FF));
    lv_style_set_shadow_opa(&style_glow_line, LV_OPA_60);
    lv_style_set_radius(&style_glow_line, 0);

    /* ---------- 分隔线中点装饰: 8x8 发光方点 ---------- */
    lv_style_init(&style_glow_dot);
    lv_style_set_bg_opa(&style_glow_dot, LV_OPA_COVER);
    lv_style_set_bg_color(&style_glow_dot, lv_color_hex(0x9DB4FF));
    lv_style_set_shadow_width(&style_glow_dot, 10);
    lv_style_set_shadow_color(&style_glow_dot, lv_color_hex(0x9DB4FF));
    lv_style_set_shadow_opa(&style_glow_dot, LV_OPA_60);
}
