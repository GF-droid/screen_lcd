#include "page_conf.h"
#include "res_conf.h"

static lv_obj_t* lv_image_vewer_create(lv_obj_t* parent, const char* image_path, lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, image_path);
    lv_obj_align(img, align, x_ofs, y_ofs);
    return img;
}

static lv_obj_t* left_component(lv_obj_t* parent) {
    lv_obj_t* bottom = lv_image_vewer_create(parent, IMAGE_PATH "lv_image28.png", LV_ALIGN_LEFT_MID, 107, -10);

    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(bottom, 5, LV_PART_MAIN);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_left_top.png", LV_ALIGN_TOP_MID, 0, 0);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_left.png", LV_ALIGN_TOP_MID, 0, 0);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_left.png", LV_ALIGN_TOP_MID, 0, 0 * 2);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_left.png", LV_ALIGN_TOP_MID, 0, 0 * 3);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_left.png", LV_ALIGN_TOP_MID, 0, 0 * 4);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_left_bottom.png", LV_ALIGN_BOTTOM_MID, 0, 0);

    return bottom;
}

static lv_obj_t* right_component(lv_obj_t* parent) {
    /* 与 left_component 对称: 容器底图同为 lv_image28.png */
    lv_obj_t* bottom = lv_image_vewer_create(parent, IMAGE_PATH "lv_image28.png", LV_ALIGN_RIGHT_MID, -107, -10);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_right_top.png", LV_ALIGN_TOP_MID, 0, 0);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_right.png", LV_ALIGN_TOP_MID, 0, 29);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_right.png", LV_ALIGN_TOP_MID, 0, 29 * 2);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_right.png", LV_ALIGN_TOP_MID, 0, 29 * 3);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_right.png", LV_ALIGN_TOP_MID, 0, 29 * 4);

    lv_image_vewer_create(bottom, IMAGE_PATH "lv_image_right_bottom.png", LV_ALIGN_BOTTOM_MID, 0, 0);

    return bottom;
}

/**
 * @file settings_list.c
 * @brief 深色玻璃质感设置列表（面板 + 多行 + 选中行发光高亮）实现，适配 LVGL v9
 */

/* -------------------- 静态样式（整个程序生命周期只初始化一次） -------------------- */
static lv_style_t style_panel;        /* 外层面板 */
static lv_style_t style_row;          /* 每一行的容器（含底部分隔线） */
static lv_style_t style_label_left;   /* 左侧文字 */
static lv_style_t style_label_right;  /* 右侧文字 */
static lv_style_t style_highlight_bg; /* 选中行：白色/浅紫渐变高亮条 */
static lv_style_t style_glow_blob;    /* 选中行：两端彩色光晕团（只显示阴影，不显示实体） */
static bool settings_style_inited = false;

static void settings_list_style_init(void) {
    if (settings_style_inited)
        return;
    settings_style_inited = true;

    /* ---------- 外层面板 ---------- */
    lv_style_init(&style_panel);
    lv_style_set_radius(&style_panel, 20);
    lv_style_set_bg_opa(&style_panel, LV_OPA_COVER);
    lv_style_set_bg_color(&style_panel, lv_color_hex(0x1E2E5E));      /* 面板-顶部 */
    lv_style_set_bg_grad_color(&style_panel, lv_color_hex(0x0B1430)); /* 面板-底部 */
    lv_style_set_bg_grad_dir(&style_panel, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_panel, 1);
    lv_style_set_border_color(&style_panel, lv_color_hex(0x9DB4FF));
    lv_style_set_border_opa(&style_panel, LV_OPA_30);
    lv_style_set_shadow_width(&style_panel, 20);
    lv_style_set_shadow_color(&style_panel, lv_color_hex(0x6C7CFF));
    lv_style_set_shadow_opa(&style_panel, LV_OPA_30);
    lv_style_set_pad_all(&style_panel, 0);
    lv_style_set_clip_corner(&style_panel, true); /* 行内容不会露出圆角外面 */
    /* 面板本身用纵向 flex 布局，行按顺序往下排 */
    lv_style_set_layout(&style_panel, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&style_panel, LV_FLEX_FLOW_COLUMN);

    /* ---------- 每一行 ---------- */
    lv_style_init(&style_row);
    lv_style_set_width(&style_row, LV_PCT(100));
    lv_style_set_bg_opa(&style_row, LV_OPA_TRANSP);
    lv_style_set_border_side(&style_row, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(&style_row, 1);
    lv_style_set_border_color(&style_row, lv_color_hex(0xAAB8FF));
    lv_style_set_border_opa(&style_row, LV_OPA_20); /* 淡淡的行分隔线 */
    lv_style_set_pad_left(&style_row, 24);
    lv_style_set_pad_right(&style_row, 24);
    lv_style_set_pad_column(&style_row, 14); /* 图标和左侧文字之间的间距 */
    lv_style_set_radius(&style_row, 0);
    /* 行内用横向 flex 布局摆放 [图标] [左侧文字]，右侧文字单独手动定位 */
    lv_style_set_layout(&style_row, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&style_row, LV_FLEX_FLOW_ROW);
    lv_style_set_flex_main_place(&style_row, LV_FLEX_ALIGN_START);
    lv_style_set_flex_cross_place(&style_row, LV_FLEX_ALIGN_CENTER);

    /* ---------- 左右文字 ---------- */
    lv_style_init(&style_label_left);
    lv_style_set_text_color(&style_label_left, lv_color_hex(0xE6ECFF));
    lv_style_set_text_font(&style_label_left, &lv_font_montserrat_16);

    lv_style_init(&style_label_right);
    lv_style_set_text_color(&style_label_right, lv_color_hex(0xE6ECFF));
    lv_style_set_text_font(&style_label_right, &lv_font_montserrat_16);

    /* ---------- 选中行：高亮条 ---------- */
    lv_style_init(&style_highlight_bg);
    lv_style_set_radius(&style_highlight_bg, 14);
    lv_style_set_bg_opa(&style_highlight_bg, LV_OPA_COVER);
    lv_style_set_bg_color(&style_highlight_bg, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_grad_color(&style_highlight_bg, lv_color_hex(0xE1E6FF));
    lv_style_set_bg_grad_dir(&style_highlight_bg, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_highlight_bg, 1);
    lv_style_set_border_color(&style_highlight_bg, lv_color_white());
    lv_style_set_border_opa(&style_highlight_bg, LV_OPA_60);

    /* ---------- 选中行：两端彩色光晕团（透明背景，只留阴影发光） ---------- */
    lv_style_init(&style_glow_blob);
    lv_style_set_radius(&style_glow_blob, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_glow_blob, LV_OPA_TRANSP);
    lv_style_set_shadow_width(&style_glow_blob, 36);
    lv_style_set_shadow_spread(&style_glow_blob, 4);
    lv_style_set_shadow_opa(&style_glow_blob, LV_OPA_70);
    lv_style_set_border_width(&style_glow_blob, 0);
}

lv_obj_t* settings_panel_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
    settings_list_style_init();

    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &style_panel, LV_STATE_DEFAULT);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_scrollable(panel, false);

    return panel;
}

settings_row_t settings_row_add(lv_obj_t* panel, lv_coord_t row_h, const char* left_text, const char* right_text) {
    settings_row_t row = {0};
    lv_coord_t panel_w = lv_obj_get_width(panel);

    /* 1. 行容器 */
    row.row = lv_obj_create(panel);
    lv_obj_remove_style_all(row.row);
    lv_obj_add_style(row.row, &style_row, LV_STATE_DEFAULT);
    lv_obj_set_height(row.row, row_h);
    lv_obj_set_scrollable(row.row, false);

    /* 2. 选中态高亮层：先创建、放最底层，这样文字/图标始终盖在它上面 */
    row.highlight = lv_obj_create(row.row);
    lv_obj_remove_style_all(row.highlight);
    lv_obj_set_size(row.highlight, LV_PCT(100), row_h - 12);
    lv_obj_center(row.highlight);
    lv_obj_set_ignore_layout(row.highlight, true); /* 不参与行的 flex 布局，手动定位 */
    lv_obj_set_clickable(row.highlight, false);
    lv_obj_set_scrollable(row.highlight, false);
    lv_obj_set_style_bg_opa(row.highlight, LV_OPA_TRANSP, LV_STATE_DEFAULT); /* 高亮层本身透明，真正的条和光晕是它的子对象 */

    /*   2.1 高亮条本体 */
    lv_obj_t* hl_bar = lv_obj_create(row.highlight);
    lv_obj_remove_style_all(hl_bar);
    lv_obj_add_style(hl_bar, &style_highlight_bg, LV_STATE_DEFAULT);
    lv_obj_set_size(hl_bar, LV_PCT(100), LV_PCT(100));
    lv_obj_center(hl_bar);
    lv_obj_set_clickable(hl_bar, false);
    lv_obj_set_scrollable(hl_bar, false);

    /*   2.2 左端蓝色光晕团 */
    lv_obj_t* blob_l = lv_obj_create(row.highlight);
    lv_obj_remove_style_all(blob_l);
    lv_obj_add_style(blob_l, &style_glow_blob, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(blob_l, lv_color_hex(0x5CC8FF), LV_STATE_DEFAULT); /* 青蓝色 */
    lv_obj_set_size(blob_l, 30, 30);
    lv_obj_align(blob_l, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_clickable(blob_l, false);
    lv_obj_set_scrollable(blob_l, false);

    /*   2.3 右端紫色光晕团 */
    lv_obj_t* blob_r = lv_obj_create(row.highlight);
    lv_obj_remove_style_all(blob_r);
    lv_obj_add_style(blob_r, &style_glow_blob, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(blob_r, lv_color_hex(0xB56CFF), LV_STATE_DEFAULT); /* 紫色 */
    lv_obj_set_size(blob_r, 30, 30);
    lv_obj_align(blob_r, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_clickable(blob_r, false);
    lv_obj_set_scrollable(blob_r, false);

    /* 默认不选中：隐藏整个高亮层 */
    lv_obj_set_hidden(row.highlight, true);

    /* 3. 图标槽：空容器，图案自己加 */
    row.icon_slot = lv_obj_create(row.row);
    lv_obj_remove_style_all(row.icon_slot);
    lv_obj_set_size(row.icon_slot, 32, 32);
    lv_obj_set_style_bg_opa(row.icon_slot, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_clickable(row.icon_slot, false);
    lv_obj_set_scrollable(row.icon_slot, false);

    /* 4. 左侧文字（跟在图标槽后面，由行的 flex 布局自动排列） */
    row.label_left = lv_label_create(row.row);
    lv_obj_add_style(row.label_left, &style_label_left, LV_STATE_DEFAULT);
    lv_label_set_text(row.label_left, left_text);

    /* 5. 右侧文字：手动定位到面板宽度的约 55% 处，形成第二列 */
    row.label_right = lv_label_create(row.row);
    lv_obj_add_style(row.label_right, &style_label_right, LV_STATE_DEFAULT);
    lv_label_set_text(row.label_right, right_text);
    lv_obj_set_ignore_layout(row.label_right, true);
    lv_obj_align(row.label_right, LV_ALIGN_LEFT_MID, (lv_coord_t)(panel_w * 0.55f) - 24, 0);

    return row;
}

void settings_row_set_selected(settings_row_t* row, bool selected) {
    if (selected)
        lv_obj_set_hidden(row->highlight, false);
    else
        lv_obj_set_hidden(row->highlight, true);
}

void set_init(void) {
    lv_image_vewer_create(lv_screen_active(), IMAGE_PATH "back.png", LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_image_vewer_create(lv_screen_active(), IMAGE_PATH "top.png", LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* bottom_img = lv_image_vewer_create(lv_screen_active(), IMAGE_PATH "bottom.png", LV_ALIGN_BOTTOM_MID, 0, 0);
    // lv_obj_t* btn = lv_button_create(bottom_img);
    // lv_obj_set_size(btn, 140, 45);
    // lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    // lv_obj_set_style_radius(btn, 30, LV_PART_MAIN);

    // lv_obj_t* bottom = left_component(lv_screen_active());

    lv_obj_t* panel = settings_panel_create(lv_scr_act(), 700, 460);
lv_obj_center(panel);  // 顺便建议加上，见下面第2点

lv_font_t* font = get_font(FONT_TYPE_CN, 32);
lv_obj_set_style_text_font(panel, font, LV_STATE_DEFAULT);
settings_row_t row_bt    = settings_row_add(panel, 90, "蓝牙设置",   "显示与亮度");
settings_row_t row_disp  = settings_row_add(panel, 90, "显示与亮度", "声音设置");
settings_row_t row_alarm = settings_row_add(panel, 90, "闹钟管理",   "闹钟管理");
settings_row_t row_log   = settings_row_add(panel, 90, "日志记录",   "系统更新");

settings_row_set_selected(&row_alarm, true);

    lv_obj_t* btn = glass_glow_btn_create(bottom_img, 140, 45, "shure");
    lv_obj_center(btn);
}
