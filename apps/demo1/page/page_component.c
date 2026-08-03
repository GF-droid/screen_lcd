#include "page_conf.h"

static lv_style_t style_ring_main;    /* 第 2 层：深色圆环，默认态（同时也是可点击的按键本体） */
static lv_style_t style_ring_pressed; /* 第 2 层：深色圆环，按下态 */
static lv_style_t style_pill_main;    /* 第 3 层：玻璃质感渐变主体 */
static lv_style_t style_highlight;    /* 第 3 层：顶部高光 */
static lv_grad_dsc_t grad_highlight;  /* 高光渐变描述符：必须 static/全局，按键存活期间不能销毁 */
static bool style_inited = false;

void glass_glow_btn_style_init(void) {
    if (style_inited)
        return;
    style_inited = true;

    /* ---------- 第 2 层：深色圆环（默认态） ---------- */
    lv_style_init(&style_ring_main);
    lv_style_set_radius(&style_ring_main, LV_RADIUS_CIRCLE);

    /* 圆环本身也带一点竖直渐变，比主体颜色更深、更饱和 */
    lv_style_set_bg_opa(&style_ring_main, LV_OPA_COVER);
    lv_style_set_bg_color(&style_ring_main, lv_color_hex(0x6169D6));      /* 环-顶部 */
    lv_style_set_bg_grad_color(&style_ring_main, lv_color_hex(0x3B44A8)); /* 环-底部，更深更饱和 */
    lv_style_set_bg_grad_dir(&style_ring_main, LV_GRAD_DIR_VER);

    /* 外发光光晕：颜色偏浅一些的紫蓝色，大范围模糊扩散 */
    lv_style_set_shadow_width(&style_ring_main, 24);
    lv_style_set_shadow_spread(&style_ring_main, 2);
    lv_style_set_shadow_color(&style_ring_main, lv_color_hex(0x8C99E6));
    lv_style_set_shadow_opa(&style_ring_main, LV_OPA_60);

    lv_style_set_pad_all(&style_ring_main, 0);

    /* ---------- 第 2 层：深色圆环（按下态） ---------- */
    lv_style_init(&style_ring_pressed);
    lv_style_set_bg_color(&style_ring_pressed, lv_color_hex(0x4E57C0));
    lv_style_set_bg_grad_color(&style_ring_pressed, lv_color_hex(0x2E3690));
    lv_style_set_shadow_opa(&style_ring_pressed, LV_OPA_90);
    lv_style_set_shadow_width(&style_ring_pressed, 30);

    /* ---------- 第 3 层：玻璃质感渐变主体 ---------- */
    lv_style_init(&style_pill_main);
    lv_style_set_radius(&style_pill_main, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_pill_main, LV_OPA_COVER);
    lv_style_set_bg_color(&style_pill_main, lv_color_hex(0xDCE0FF));      /* 主体-顶部，浅蓝白 */
    lv_style_set_bg_grad_color(&style_pill_main, lv_color_hex(0x4A46D6)); /* 主体-底部，靛蓝紫 */
    lv_style_set_bg_grad_dir(&style_pill_main, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_pill_main, 0); /* 描边职责交给圆环层，主体不再单独描边 */
    lv_style_set_text_color(&style_pill_main, lv_color_white());
    lv_style_set_text_font(&style_pill_main, &lv_font_montserrat_16);
    lv_style_set_pad_all(&style_pill_main, 0);

    /* ---------- 第 3 层：顶部高光，真正的"不透明 -> 全透明"渐隐 ---------- */
    lv_color_t hl_colors[2] = {lv_color_white(), lv_color_white()};
    lv_opa_t hl_opas[2] = {LV_OPA_70, LV_OPA_TRANSP};
    lv_grad_init_stops(&grad_highlight, hl_colors, hl_opas, NULL, 2);
    grad_highlight.dir = LV_GRAD_DIR_VER;

    lv_style_init(&style_highlight);
    lv_style_set_radius(&style_highlight, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_highlight, LV_OPA_COVER);
    lv_style_set_bg_grad(&style_highlight, &grad_highlight);
    lv_style_set_border_width(&style_highlight, 0);
}

/**
 * @brief 创建一个玻璃质感 / 外发光胶囊按键（带深色圆环镶边）
 * @param parent 父对象
 * @param w      按键总宽度（含圆环）
 * @param h      按键总高度（含圆环）
 * @param text   按键文案
 * @return 创建的好按键对象 (lv_obj_t *)，即可点击的圆环层对象；
 *         可继续对其调用 lv_obj_add_event_cb / lv_obj_align 等
 */
lv_obj_t* glass_glow_btn_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, const char* text) {
    glass_glow_btn_style_init();

    /* 圆环层的厚度：随按键高度自适应，最小 2px */
    lv_coord_t ring_th = h / 12;
    if (ring_th < 2)
        ring_th = 2;

    /* 1. 圆环层（= 返回的按键对象本体，真正响应点击） */
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_add_style(btn, &style_ring_main, LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &style_ring_pressed, LV_STATE_PRESSED);
    lv_obj_set_scrollable(btn, false);
    lv_obj_set_style_clip_corner(btn, true, LV_STATE_DEFAULT);

    /* 2. 玻璃质感主体：在圆环内侧收缩 ring_th，露出外圈的深色圆环 */
    lv_obj_t* pill = lv_obj_create(btn);
    lv_obj_remove_style_all(pill);
    lv_obj_add_style(pill, &style_pill_main, LV_STATE_DEFAULT);
    lv_obj_set_size(pill, w - ring_th * 2, h - ring_th * 2);
    lv_obj_center(pill);
    lv_obj_set_clickable(pill, false);
    lv_obj_set_scrollable(pill, false);
    lv_obj_set_style_clip_corner(pill, true, LV_STATE_DEFAULT);

    lv_coord_t pill_w = w - ring_th * 2;
    lv_coord_t pill_h = h - ring_th * 2;

    /* 3. 顶部高光条：只覆盖主体上部约 30% 左右的"光泽弧"，靠透明度渐变自然淡出 */
    lv_obj_t* highlight = lv_obj_create(pill);
    lv_obj_remove_style_all(highlight);
    lv_obj_add_style(highlight, &style_highlight, LV_STATE_DEFAULT);
    lv_coord_t hl_w = (lv_coord_t)(pill_w * 0.92f);
    lv_coord_t hl_h = (lv_coord_t)(pill_h * 0.55f);
    lv_obj_set_size(highlight, hl_w, hl_h);
    lv_obj_align(highlight, LV_ALIGN_TOP_MID, 0, -(lv_coord_t)(pill_h * 0.15f));
    lv_obj_set_clickable(highlight, false);
    lv_obj_set_scrollable(highlight, false);
    lv_obj_move_background(highlight);

    /* 4. 文案标签 */
    lv_obj_t* label = lv_label_create(pill);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return btn;
}