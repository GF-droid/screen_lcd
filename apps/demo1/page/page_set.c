#include "page_conf.h"
#include "res_conf.h"

static lv_obj_t* lv_image_vewer_create(lv_obj_t* parent, const char* image_path, lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, image_path);
    lv_obj_align(img, align, x_ofs, y_ofs);
    return img;
}

static lv_style_t style_chip;       /* 气泡本体 */
static lv_style_t style_chip_label; /* 气泡文案 */
static bool chip_bubble_style_inited = false;
 
static void chip_bubble_style_init(void) {
    if (chip_bubble_style_inited) return;
    chip_bubble_style_inited = true;
 
    /* ---------- 气泡本体：玻璃质感渐变胶囊 ---------- */
    lv_style_init(&style_chip);
    lv_style_set_radius(&style_chip, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_chip, LV_OPA_80);
    lv_style_set_bg_color(&style_chip, lv_color_hex(0x6B7BFF));      /* 气泡-顶部 */
    lv_style_set_bg_grad_color(&style_chip, lv_color_hex(0x3A46B8)); /* 气泡-底部 */
    lv_style_set_bg_grad_dir(&style_chip, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_chip, 1);
    lv_style_set_border_color(&style_chip, lv_color_white());
    lv_style_set_border_opa(&style_chip, LV_OPA_30);
    lv_style_set_shadow_width(&style_chip, 10);
    lv_style_set_shadow_color(&style_chip, lv_color_hex(0x7C86FF));
    lv_style_set_shadow_opa(&style_chip, LV_OPA_20);
    lv_style_set_pad_left(&style_chip, 14);
    lv_style_set_pad_right(&style_chip, 14);
    lv_style_set_pad_column(&style_chip, 10); /* 图标槽和文字之间的间距 */
    /* 内部横向排列：[图标槽] [文案] */
    lv_style_set_layout(&style_chip, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&style_chip, LV_FLEX_FLOW_ROW);
    lv_style_set_flex_main_place(&style_chip, LV_FLEX_ALIGN_START);
    lv_style_set_flex_cross_place(&style_chip, LV_FLEX_ALIGN_CENTER);
 
    /* ---------- 气泡文案（不设字体，跟随外部设置继承） ---------- */
    lv_style_init(&style_chip_label);
    lv_style_set_text_color(&style_chip_label, lv_color_white());
}
 
lv_obj_t * chip_bubble_create(lv_obj_t * parent, lv_coord_t w, lv_coord_t h, const char * text) {
    chip_bubble_style_init();
 
    /* 1. 气泡本体 */
    lv_obj_t * chip = lv_obj_create(parent);
    lv_obj_remove_style_all(chip);
    lv_obj_add_style(chip, &style_chip, LV_STATE_DEFAULT);
    lv_obj_set_size(chip, w, h);
    lv_obj_set_scrollable(chip, false);
 
    /* 布局解析一次，拿到气泡真实的像素高度，图标槽尺寸按它算比例 */
    lv_obj_update_layout(chip);
    lv_coord_t chip_h_px = lv_obj_get_height(chip);
 
    /* 2. 图标槽：空的正方形容器，边长是气泡高度的比例，图标自己加 */
    lv_coord_t icon_size = chip_h_px * 55 / 100; /* ≈ 气泡高度的 55% (整数运算) */
    lv_obj_t * icon_slot = lv_obj_create(chip);
    lv_obj_remove_style_all(icon_slot);
    lv_obj_set_size(icon_slot, icon_size, icon_size);
    lv_obj_set_style_bg_opa(icon_slot, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_clickable(icon_slot, false);
    lv_obj_set_scrollable(icon_slot, false);
 
    /* 3. 文案 */
    lv_obj_t * label = lv_label_create(chip);
    lv_obj_add_style(label, &style_chip_label, LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_clickable(label, false);
 
    return chip;
}

void set_init(void) {
    lv_image_vewer_create(lv_screen_active(), IMAGE_PATH "back.png", LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_image_vewer_create(lv_screen_active(), IMAGE_PATH "top.png", LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* bottom_img = lv_image_vewer_create(lv_screen_active(), IMAGE_PATH "bottom.png", LV_ALIGN_BOTTOM_MID, 0, 0);
    
    font_init(); /* 注册 TTF 字体路径, 必须在 get_font() 之前 */
    
    lv_obj_t* panel = settings_panel_create(lv_scr_act(), LV_PCT(20), LV_PCT(65));
    lv_obj_align_to(panel, lv_scr_act(), LV_ALIGN_LEFT_MID, 107, 0);
    lv_obj_update_layout(panel);

    lv_font_t* font = get_font(FONT_TYPE_CN, 10);
    if (font != NULL) {
        lv_obj_set_style_text_font(panel, font, LV_STATE_DEFAULT);
    }
    /* TODO: 把你的图标 PNG 放到 res/image/ 下, 把 NULL 换成 IMAGE_PATH "你的图标.png" */
    settings_row_t row_bt = row_add_clickable(panel, IMAGE_PATH "iconfont_bule.png", "蓝牙设置", "显示与亮度");
    settings_row_t row_disp = row_add_clickable(panel, IMAGE_PATH "iconfont_yinl.png", "显示与亮度", "声音设置");
    settings_row_t row_alarm = row_add_clickable(panel, IMAGE_PATH "iconfont_laoz.png", "闹钟管理", "闹钟管理");
    settings_row_t row_log = row_add_clickable(panel, IMAGE_PATH "iconfont_riz.png", "日志记录", "系统更新");
    settings_row_t row_update = row_add_clickable(panel, IMAGE_PATH "iconfont_genx.png", "系统更新", "关于");

    settings_row_set_selected(&row_alarm, true); /* 初始选中第 3 行 */

    /* 状态卡片: 屏幕居中 (create 内部已自动初始化样式, 无需再调 style_init) */
    lv_obj_t* sp_panel = status_panel_create(lv_scr_act(), LV_PCT(60), LV_PCT(60)).panel;
    lv_obj_align_to(sp_panel, panel, LV_ALIGN_OUT_RIGHT_MID, 86, 0);

    lv_font_t* fontback = get_font(FONT_TYPE_CN, 30);
    if (fontback != NULL) {
        lv_obj_set_style_text_font(sp_panel, fontback, LV_STATE_DEFAULT);
    }

    chip_bubble_create(sp_panel, 140, 40, "正常");

    lv_obj_t* btn = glass_glow_btn_create(bottom_img, 140, 45, "shure");
    lv_obj_center(btn);
}
