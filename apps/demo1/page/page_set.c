#include "page_conf.h"
#include "res_conf.h"

static lv_obj_t* lv_image_vewer_create(lv_obj_t* parent, const char* image_path, lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, image_path);
    lv_obj_align(img, align, x_ofs, y_ofs);
    return img;
}

static lv_obj_t* lv_image_vewer_create_with_size(lv_obj_t* parent, const char* image_path, lv_coord_t w, lv_coord_t h, char* text) {
    lv_obj_t* lv_img = chip_bubble_create(parent, w, h, text);
    lv_obj_t* img = lv_image_create(lv_img);
    lv_image_set_src(img, image_path);
    lv_coord_t src_w = lv_image_get_src_width(img); /* 原图宽度 */
    if (src_w > 0) {
        lv_image_set_scale(img, 256 * 16 / src_w); /* 按宽度缩到 16px, 高度等比 */
    }

    lv_obj_set_ignore_layout(img, true); /* 不参与 flex 布局, 由手动 align 定位 */
    lv_obj_align_to(img, lv_img, LV_ALIGN_LEFT_MID, -5, 0);
    return lv_img;
}

static lv_obj_t* lv_mid_screen_componnet_create(lv_obj_t* parent) {
    /* 状态卡片: 靠屏幕右半区 (卡片 60% 宽, 从右侧向内收, 不与左侧设置列表重叠) */
    lv_obj_t* sp_panel = status_panel_create(parent, LV_PCT(60), LV_PCT(60));
    lv_obj_align(sp_panel, LV_ALIGN_RIGHT_MID, -20, 0);

    lv_obj_t* label = lv_label_create(sp_panel);
    lv_font_t* font = get_font(FONT_TYPE_CN, 30);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, LV_STATE_DEFAULT);
    }
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(label, "状态卡片");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, -10);

    /* 深色内容底板: 卡片无布局, 直接手动对齐 */
    lv_obj_t* temp = lv_obj_create(sp_panel);
    lv_obj_remove_style_all(temp);
    lv_obj_set_size(temp, LV_PCT(100), LV_PCT(80));
    lv_obj_align(temp, LV_ALIGN_BOTTOM_MID, 0, 15);
    lv_obj_set_clickable(temp, false);
    lv_obj_set_scrollable(temp, false);
    /* flex 自动换行: 从左往右排, 一行放不下自动换下一行, 间距 10px */
    lv_obj_set_style_layout(temp, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(temp, LV_FLEX_FLOW_ROW_WRAP, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(temp, 25, LV_STATE_DEFAULT); /* 列间距 */
    lv_obj_set_style_pad_row(temp, 25, LV_STATE_DEFAULT);    /* 行间距 */

    lv_font_t* fontback = get_font(FONT_TYPE_CN, 10);
    if (fontback != NULL) {
        lv_obj_set_style_text_font(sp_panel, fontback, LV_STATE_DEFAULT);
    }
    /* 气泡由 flex 自动排列成行, 无需手动定位 */
    lv_obj_t* chip0 = lv_image_vewer_create_with_size(temp, IMAGE_PATH "iconfont_wifi.png", 140, 30, "WIFI办公室");
    lv_obj_t* chip1 = lv_image_vewer_create_with_size(temp, IMAGE_PATH "lany.png", 140, 30, "蓝牙办公室");
    lv_obj_t* chip2 = lv_image_vewer_create_with_size(temp, IMAGE_PATH "iconfont_ip.png", 140, 30, "IP地址:127.0.0.1");
    lv_obj_t* chip3 = lv_image_vewer_create_with_size(temp, IMAGE_PATH "ceshi.png", 140, 30, "网络测试");
    lv_obj_t* chip4 = lv_image_vewer_create_with_size(temp, IMAGE_PATH "xianshima.png", 140, 30, "子掩饰码");
    lv_obj_t* chip5 = lv_image_vewer_create_with_size(temp, IMAGE_PATH "iconfont_guan.png", 140, 30, "待定");
    lv_obj_t* chip6 = lv_image_vewer_create_with_size(temp, IMAGE_PATH "iconfont_bule.png", 140, 30, "待定");
    lv_obj_t* chip7 = lv_image_vewer_create_with_size(temp, IMAGE_PATH "iconfont_guan.png", 140, 30, "待定");
    // lv_obj_t* chip8 = chip_bubble_create(temp, 140, 40, "正常");
    return sp_panel;
}

/* ============ 顶部组件: 第一部分 - 盒子创建 ============ */
/* 创建顶部背景图 + flex 图标容器, 返回容器供 add_info 使用 */
static lv_obj_t* top_component_create(lv_obj_t* parent) {
    lv_obj_t* top_img = lv_image_vewer_create(parent, IMAGE_PATH "top.png", LV_ALIGN_TOP_MID, 0, 0);

    /* 图标容器: flex 横向排布 */
    lv_obj_t* temp = lv_obj_create(top_img);
    lv_obj_remove_style_all(temp);
    lv_obj_set_size(temp, LV_PCT(50), LV_PCT(100));
    lv_obj_set_style_layout(temp, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);      /* 开启 flex */
    lv_obj_set_style_flex_flow(temp, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT); /* 横向 */
    lv_obj_align(temp, LV_ALIGN_CENTER, 0, 0);

    return temp; /* 注意: 返回的是 flex 容器, 不是背景图 */
}

/* ============ 顶部组件: 第二部分 - 显示信息添加 ============ */
typedef struct {
    const char* icon_path; /* 图标图片路径 */
    const char* text;      /* 文案 */
} top_info_item_t;

/* 往顶部容器里添加 [图标 + 文案] 条目, 每个 flex_grow(1) 均分容器宽度 */
static void top_component_add_info(lv_obj_t* container) {
    static const top_info_item_t items[] = {
        {IMAGE_PATH "iconfont_shijian.png", "时间:13:45"},
        {IMAGE_PATH "iconfont_wifi.png", "WIFI:已连接"},
        {IMAGE_PATH "lany.png", "蓝牙:已连接"},
        {IMAGE_PATH "iconfont_yinl.png", "音量:50%"},
    };

    lv_font_t* font = get_font(FONT_TYPE_CN, 16); /* 顶栏文案字体 (中文必需) */
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        lv_obj_t* box = lv_obj_create(container);
        lv_obj_remove_style_all(box);
        lv_obj_set_size(box, LV_PCT(20), LV_PCT(100));

        lv_obj_t* icon = lv_image_vewer_create(box, items[i].icon_path, LV_ALIGN_LEFT_MID, 80, 0);
        lv_obj_t* label = lv_label_create(box);
        lv_label_set_text(label, items[i].text);
        if (font != NULL) {
            lv_obj_set_style_text_font(label, font, LV_STATE_DEFAULT);
        }
        lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT); /* 默认黑色字在深色栏上看不见 */
        lv_obj_align_to(label, icon, LV_ALIGN_LEFT_MID, 30, 0);

        lv_obj_set_flex_grow(box, 1); /* 均分的关键 */
    }
}

void set_init(void) {
    font_init(); /* 注册 TTF 字体路径, 必须在任何 get_font() 之前 */

    lv_image_vewer_create(lv_screen_active(), IMAGE_PATH "back.png", LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t* top_box = top_component_create(lv_screen_active()); /* 第一部分: 盒子 */
    top_component_add_info(top_box);                              /* 第二部分: 显示信息 */

    lv_obj_t* bottom_img = lv_image_vewer_create(lv_screen_active(), IMAGE_PATH "bottom.png", LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t* panel = settings_panel_create(lv_scr_act(), LV_PCT(20), LV_PCT(65));
    lv_obj_align_to(panel, lv_scr_act(), LV_ALIGN_LEFT_MID, 107, 0);
    lv_obj_update_layout(panel);

    lv_font_t* font = get_font(FONT_TYPE_CN, 10);
    if (font != NULL) {
        lv_obj_set_style_text_font(panel, font, LV_STATE_DEFAULT);
    }
    /* TODO: 把你的图标 PNG 放到 res/image/ 下, 把 NULL 换成 IMAGE_PATH "你的图标.png" */
    settings_row_t row_bt = row_add_clickable(panel, IMAGE_PATH "lany.png", "蓝牙设置", "显示与亮度");
    settings_row_t row_disp = row_add_clickable(panel, IMAGE_PATH "wangl.png", "网络连接 ", "网络设置");
    settings_row_t row_alarm = row_add_clickable(panel, IMAGE_PATH "iconfont_laoz.png", "闹钟管理", "闹钟管理");
    settings_row_t row_log = row_add_clickable(panel, IMAGE_PATH "iconfont_riz.png", "日志记录", "系统更新");
    settings_row_t row_update = row_add_clickable(panel, IMAGE_PATH "iconfont_genx.png", "系统更新", "关于");

    settings_row_set_selected(&row_alarm, true); /* 初始选中第 3 行 */

    lv_obj_t* sp_panel = lv_mid_screen_componnet_create(lv_screen_active());
    lv_obj_align_to(sp_panel, panel, LV_ALIGN_OUT_RIGHT_MID, 66, 0);

    lv_obj_t* btn = glass_glow_btn_create(bottom_img, 140, 45, "返回");
    lv_obj_center(btn);

    /* 字体设在按钮对象上, 经继承链传给内部文案 label */
    lv_font_t* font_btn = get_font(FONT_TYPE_CN, 30);
    if (font_btn != NULL) {
        lv_obj_set_style_text_font(btn, font_btn, LV_STATE_DEFAULT);
    }
}
