#include "widgets/widget_common.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
    /* 注意: 这里不设 text_font, 字体由外部在按钮对象上设置后继承 */
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
    lv_coord_t hl_w = pill_w * 92 / 100; /* 高光条宽 ≈ 主体 92% (整数运算) */
    lv_coord_t hl_h = pill_h * 55 / 100; /* 高光条高 ≈ 主体 55% */
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

/* -------------------- 设置列表组件: 静态样式（整个程序生命周期只初始化一次） -------------------- */
static lv_style_t style_panel;       /* 外层面板 */
static lv_style_t style_row;         /* 每一行的容器（含底部分隔线） */
static lv_style_t style_row_sel;     /* 行选中态: 整行半透明提亮背景 */
static lv_style_t style_label_left;  /* 左侧文字 */
static lv_style_t style_label_right; /* 右侧文字 */
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
    lv_style_set_pad_top(&style_row, 6);
    lv_style_set_pad_bottom(&style_row, 6);
    lv_style_set_pad_column(&style_row, 14); /* 图标和左侧文字之间的间距 */
    lv_style_set_radius(&style_row, 0);
    /* 行内用横向 flex 布局摆放 [图标] [左侧文字]，右侧文字单独手动定位 */
    lv_style_set_layout(&style_row, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&style_row, LV_FLEX_FLOW_ROW);
    lv_style_set_flex_main_place(&style_row, LV_FLEX_ALIGN_START);
    lv_style_set_flex_cross_place(&style_row, LV_FLEX_ALIGN_CENTER);

    /* ---------- 左右文字 ---------- */
    /* 注意: 这里不设置 text_font, 字体从父对象 (panel) 继承,
       这样外部只需在 panel 上设置一次 get_font() 的字体即可 */
    lv_style_init(&style_label_left);
    lv_style_set_text_color(&style_label_left, lv_color_hex(0xE6ECFF));

    lv_style_init(&style_label_right);
    lv_style_set_text_color(&style_label_right, lv_color_hex(0xE6ECFF));

    /* ---------- 选中态：整行半透明提亮 (行背景, 天然覆盖整行含 padding, 不遮文字) ---------- */
    lv_style_init(&style_row_sel);
    lv_style_set_bg_opa(&style_row_sel, LV_OPA_40); /* 40% 提亮, 底下渐变面板仍透出 */
    lv_style_set_bg_color(&style_row_sel, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_grad_color(&style_row_sel, lv_color_hex(0xC9D4FF));
    lv_style_set_bg_grad_dir(&style_row_sel, LV_GRAD_DIR_VER);
}

/* -------------------- 设置面板组件 -------------------- */
lv_obj_t* settings_panel_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
    settings_list_style_init();

    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &style_panel, LV_STATE_DEFAULT);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_scrollable(panel, false);

    return panel;
}

/* -------------------- 设置行组件 -------------------- */
settings_row_t settings_row_add(lv_obj_t* panel, const char* icon_path, const char* left_text, const char* right_text) {
    settings_row_t row = {0};
    lv_coord_t panel_w = lv_obj_get_width(panel);

    /* 1. 行容器: 高度由面板剩余空间均分 (所有行 flex_grow=1), 行内元素垂直居中 */
    row.row = lv_obj_create(panel);
    lv_obj_remove_style_all(row.row);
    lv_obj_add_style(row.row, &style_row, LV_STATE_DEFAULT);
    lv_obj_add_style(row.row, &style_row_sel, LV_STATE_USER_1); /* 选中提亮: 行背景整行覆盖 */
    lv_obj_set_flex_grow(row.row, 1);                           /* 平分面板高度 */
    lv_obj_set_scrollable(row.row, false);

    /* 3. 图标槽 + 图标图片 (icon_path 为 NULL 时留空槽) */
    row.icon_slot = lv_obj_create(row.row);
    lv_obj_remove_style_all(row.icon_slot);
    lv_obj_set_size(row.icon_slot, 28, 28);
    lv_obj_set_style_bg_opa(row.icon_slot, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_clickable(row.icon_slot, false);
    lv_obj_set_scrollable(row.icon_slot, false);

    if (icon_path != NULL) {
        lv_obj_t* icon = lv_image_create(row.icon_slot);
        lv_image_set_src(icon, icon_path);
        lv_obj_center(icon);
    }

    /* 4. 左侧文字（跟在图标槽后面，由行的 flex 布局自动排列） */
    row.label_left = lv_label_create(row.row);
    lv_obj_add_style(row.label_left, &style_label_left, LV_STATE_DEFAULT);
    lv_label_set_text(row.label_left, left_text);
    lv_obj_set_clickable(row.label_left, false); /* 不拦截点击, 命中直接落在行容器上 */

    /* 5. 右侧文字：手动定位到面板宽度的约 55% 处，形成第二列 */
    row.label_right = lv_label_create(row.row);
    lv_obj_add_style(row.label_right, &style_label_right, LV_STATE_DEFAULT);
    lv_label_set_text(row.label_right, right_text);
    lv_obj_set_ignore_layout(row.label_right, true);
    lv_obj_align(row.label_right, LV_ALIGN_LEFT_MID, (lv_coord_t)(panel_w * 0.55f) - 24, 0);
    lv_obj_set_clickable(row.label_right, false); /* 不拦截点击, 命中直接落在行容器上 */

    return row;
}

/* 行点击互斥选中: 记住所有行, 点击某行时其他行全部取消选中 */
static settings_row_t g_rows[16];
static uint8_t g_row_cnt = 0;

static void row_click_cb(lv_event_t* e) {
    lv_obj_t* clicked = lv_event_get_target(e);
    for (uint8_t i = 0; i < g_row_cnt; i++) {
        settings_row_set_selected(&g_rows[i], g_rows[i].row == clicked);
    }
}

/* 创建一行并注册点击选中事件 */
settings_row_t row_add_clickable(lv_obj_t* panel, const char* icon_path, const char* left_text, const char* right_text) {
    settings_row_t s = settings_row_add(panel, icon_path, left_text, right_text);
    if (g_row_cnt < sizeof(g_rows) / sizeof(g_rows[0])) {
        g_rows[g_row_cnt++] = s;
        lv_obj_add_event_cb(s.row, row_click_cb, LV_EVENT_CLICKED, NULL);
    }
    return s;
}

void settings_row_set_selected(settings_row_t* row, bool selected) {
    if (selected)
        lv_obj_add_state(row->row, LV_STATE_USER_1); /* 整行提亮 */
    else
        lv_obj_clear_state(row->row, LV_STATE_USER_1);
}

/* -------------------- 状态卡片组件 -------------------- */
static lv_style_t style_card;        /* 整张卡片 */
static bool status_panel_style_inited = false;

static void status_panel_style_init(void) {
    if (status_panel_style_inited) return;
    status_panel_style_inited = true;

    /* ---------- 整张卡片：圆角渐变背景 + 发光描边 ---------- */
    lv_style_init(&style_card);
    lv_style_set_radius(&style_card, 24);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_bg_color(&style_card, lv_color_hex(0x2B2F72));      /* 卡片-顶部，偏紫 */
    lv_style_set_bg_grad_color(&style_card, lv_color_hex(0x11142E)); /* 卡片-底部，深蓝黑 */
    lv_style_set_bg_grad_dir(&style_card, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, lv_color_hex(0x9DB4FF));
    lv_style_set_border_opa(&style_card, LV_OPA_30);
    lv_style_set_shadow_width(&style_card, 26);
    lv_style_set_shadow_color(&style_card, lv_color_hex(0x7C6CFF));
    lv_style_set_shadow_opa(&style_card, LV_OPA_30);
    lv_style_set_clip_corner(&style_card, true);
    lv_style_set_pad_all(&style_card, 24);
    /* 卡片不启用布局, 子对象由外部手动 align 定位 */
}

lv_obj_t* status_panel_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
    status_panel_style_init();

    /* 卡片本体 */
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &style_card, LV_STATE_DEFAULT);
    lv_obj_set_size(card, w, h);
    lv_obj_set_scrollable(card, false);

    return card;
}

/* -------------------- 气泡胶囊组件 -------------------- */
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
    lv_obj_set_ignore_layout(label, true); /* 不参与 flex 布局，手动对齐 */
    lv_obj_update_layout(chip);            /* 先让 flex 定位好图标槽, 否则对齐会用未布局的 (0,0) 坐标 */
    lv_obj_align_to(label, icon_slot, LV_ALIGN_CENTER, 50, 7); /* 图标槽右侧 10px */
    lv_obj_set_flex_grow(label, 1); /* 已 ignore_layout, 此行使 flex 跳过它, 保留无妨 */
    lv_obj_set_clickable(label, false);

    return chip;
}

/* -------------------- 闹钟管理组件样式 (非 static, page_set.c 的 alarm_manage_create 要用) -------------------- */
lv_style_t style_alarm_roller;          /* roller 主体: 文字白色, 无背景 */
lv_style_t style_alarm_roller_sel;      /* roller 选中行: 半透明提亮 + 淡蓝描边 */
lv_style_t style_alarm_switch;          /* 闹钟开关: 关态深蓝灰 */
lv_style_t style_alarm_switch_checked;  /* 闹钟开关: 开态提亮渐变 */
lv_style_t style_alarm_small_btn;       /* 倒计时小按钮: 深蓝玻璃胶囊 */
lv_style_t style_alarm_small_btn_pressed;
lv_style_t style_alarm_small_btn_disabled;
lv_style_t style_alarm_divider;         /* 左右分栏的 1px 竖分隔线 */


static lv_style_t style_bottom_bar; /* 底部栏容器: 半透明玻璃渐变横条 */
static bool bottom_bar_style_inited = false;

static void bottom_bar_style_init(void) {
    if (bottom_bar_style_inited)
        return;
    bottom_bar_style_inited = true;

    /* 全宽横条, 直角无圆角, 颜色与面板/卡片同一色系 (深蓝紫渐变) */
    lv_style_init(&style_bottom_bar);
    lv_style_set_radius(&style_bottom_bar, 0);
    lv_style_set_bg_opa(&style_bottom_bar, LV_OPA_80);
    lv_style_set_bg_color(&style_bottom_bar, lv_color_hex(0x2A3169));      /* 顶部-偏紫蓝 */
    lv_style_set_bg_grad_color(&style_bottom_bar, lv_color_hex(0x10142E)); /* 底部-深蓝黑 */
    lv_style_set_bg_grad_dir(&style_bottom_bar, LV_GRAD_DIR_VER);

    /* 顶部 1px 淡蓝描边 + 微弱上辉光: 和面板/卡片同款描边色,
       让底部栏从背景中浮出来, 分出一条柔和的分界线 */
    lv_style_set_border_width(&style_bottom_bar, 1);
    lv_style_set_border_side(&style_bottom_bar, LV_BORDER_SIDE_TOP);
    lv_style_set_border_color(&style_bottom_bar, lv_color_hex(0x9DB4FF));
    lv_style_set_border_opa(&style_bottom_bar, LV_OPA_30);
    lv_style_set_shadow_width(&style_bottom_bar, 16);
    lv_style_set_shadow_color(&style_bottom_bar, lv_color_hex(0x6C7CFF));
    lv_style_set_shadow_opa(&style_bottom_bar, LV_OPA_20);

    lv_style_set_pad_all(&style_bottom_bar, 0);
}

/* 底部栏配套按钮: 深蓝玻璃胶囊, 与底部栏同色系, 不再用浅色玻璃 (浅色在深底上太跳) */
static lv_style_t style_bottom_btn;         /* 按钮默认态 */
static lv_style_t style_bottom_btn_pressed; /* 按钮按下态 */

static void bottom_bar_btn_style_init(void) {
    static bool inited = false;
    if (inited)
        return;
    inited = true;

    lv_style_init(&style_bottom_btn);
    lv_style_set_radius(&style_bottom_btn, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_bottom_btn, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bottom_btn, lv_color_hex(0x3A46B8));      /* 顶部-亮深蓝紫 */
    lv_style_set_bg_grad_color(&style_bottom_btn, lv_color_hex(0x222A66)); /* 底部-深蓝 */
    lv_style_set_bg_grad_dir(&style_bottom_btn, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_bottom_btn, 1);
    lv_style_set_border_color(&style_bottom_btn, lv_color_hex(0x9DB4FF));
    lv_style_set_border_opa(&style_bottom_btn, LV_OPA_40);
    lv_style_set_shadow_width(&style_bottom_btn, 10);
    lv_style_set_shadow_color(&style_bottom_btn, lv_color_hex(0x6C7CFF));
    lv_style_set_shadow_opa(&style_bottom_btn, LV_OPA_30);
    lv_style_set_text_color(&style_bottom_btn, lv_color_white());
    lv_style_set_pad_all(&style_bottom_btn, 0);

    lv_style_init(&style_bottom_btn_pressed);
    lv_style_set_bg_color(&style_bottom_btn_pressed, lv_color_hex(0x2E3690));
    lv_style_set_bg_grad_color(&style_bottom_btn_pressed, lv_color_hex(0x1A2150));
    lv_style_set_shadow_opa(&style_bottom_btn_pressed, LV_OPA_50);
}

/**
 * @brief 创建底部栏配套按钮 (深蓝玻璃胶囊, 与底部栏同色系)
 * @param bar  底部栏容器
 * @param text 按钮文案
 * @return 按钮对象, 可继续 add_event_cb / 设字体 (文字 label 继承字体)
 */
lv_obj_t* bottom_bar_btn_create(lv_obj_t* bar, const char* text) {
    bottom_bar_btn_style_init();

    lv_obj_t* btn = lv_btn_create(bar);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &style_bottom_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &style_bottom_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn, 100, 30); /* 适配 40px 高的底部栏, 上下留 5px */
    lv_obj_center(btn);
    lv_obj_set_scrollable(btn, false);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return btn;
}

/**
 * @brief 创建程序化底部栏 (替换 bottom.png)
 * @param parent 父对象 (一般是屏幕)
 * @return 底部栏容器对象, 往里面放按钮/文字即可
 */
lv_obj_t* bottom_bar_create(lv_obj_t* parent) {
    bottom_bar_style_init();

    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_add_style(bar, &style_bottom_bar, LV_STATE_DEFAULT);
    lv_obj_set_size(bar, LV_PCT(100), 40); /* 扁条: 只作底部装饰 + 承载返回按钮 */
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_scrollable(bar, false);

    return bar;
}

/* -------------------- WiFi 连接条样式 (非 static, page_set.c 的 wifi_connect_bar_create 要用) -------------------- */
lv_style_t style_bar;         /* 整条背景: 半透明深色圆角底 */
lv_style_t style_input;       /* 输入框: 深色底 + 淡蓝描边 */
lv_style_t style_btn;         /* 连接按钮: 玻璃渐变 */
lv_style_t style_btn_pressed; /* 连接按钮按下态 */
lv_style_t style_btn_label;   /* 按钮文案 */
