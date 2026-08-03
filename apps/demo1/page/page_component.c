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

static lv_style_t style_card;       /* 整张卡片 */
static lv_style_t style_content_row; /* 卡片内容区容器 */
static lv_style_t style_corner_glow; /* 卡片四角的柔和光斑，纯装饰 */
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
    /* 卡片本身纵向排布（目前只有内容区一项，占满全部空间） */
    lv_style_set_layout(&style_card, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&style_card, LV_FLEX_FLOW_COLUMN);

    /* ---------- 内容区（目前是空容器，占满剩余空间） ---------- */
    lv_style_init(&style_content_row);
    lv_style_set_width(&style_content_row, LV_PCT(100));
    lv_style_set_height(&style_content_row, LV_PCT(100));
    lv_style_set_bg_opa(&style_content_row, LV_OPA_TRANSP);
 
    /* ---------- 四角柔和光斑（纯装饰，透明背景只留阴影） ---------- */
    lv_style_init(&style_corner_glow);
    lv_style_set_radius(&style_corner_glow, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_corner_glow, LV_OPA_TRANSP);
    lv_style_set_shadow_width(&style_corner_glow, 60);
    lv_style_set_shadow_spread(&style_corner_glow, 10);
    lv_style_set_shadow_opa(&style_corner_glow, LV_OPA_30);
    lv_style_set_border_width(&style_corner_glow, 0);
}
 
status_panel_t status_panel_create(lv_obj_t * parent, lv_coord_t w, lv_coord_t h) {
    status_panel_style_init();

    status_panel_t panel = {0};

    /* 卡片本体 */
    panel.panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel.panel);
    lv_obj_add_style(panel.panel, &style_card, LV_STATE_DEFAULT);
    lv_obj_set_size(panel.panel, w, h);
    lv_obj_set_scrollable(panel.panel, false);

    /* 四角柔和光斑：一个偏左下（青蓝色）、一个偏右上（紫色），纯装饰，不参与布局 */
    lv_obj_t * glow_a = lv_obj_create(panel.panel);
    lv_obj_remove_style_all(glow_a);
    lv_obj_add_style(glow_a, &style_corner_glow, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(glow_a, lv_color_hex(0x6CC8FF), LV_STATE_DEFAULT);
    lv_obj_set_size(glow_a, 10, 10);
    lv_obj_align(glow_a, LV_ALIGN_BOTTOM_LEFT, 20, -10);
    lv_obj_set_ignore_layout(glow_a, true);
    lv_obj_set_clickable(glow_a, false);
 
    lv_obj_t * glow_b = lv_obj_create(panel.panel);
    lv_obj_remove_style_all(glow_b);
    lv_obj_add_style(glow_b, &style_corner_glow, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(glow_b, lv_color_hex(0xB56CFF), LV_STATE_DEFAULT);
    lv_obj_set_size(glow_b, 10, 10);
    lv_obj_align(glow_b, LV_ALIGN_TOP_RIGHT, -20, 10);
    lv_obj_set_ignore_layout(glow_b, true);
    lv_obj_set_clickable(glow_b, false);
 
    /* 内容区：目前是空的，占满卡片内部全部空间；
       胶囊网格（第 2 部分）会往这里面加东西 */
    panel.content = lv_obj_create(panel.panel);
    lv_obj_remove_style_all(panel.content);
    lv_obj_add_style(panel.content, &style_content_row, LV_STATE_DEFAULT);
    lv_obj_set_flex_grow(panel.content, 1); /* 内容区吃掉剩余全部空间 */
    lv_obj_set_scrollable(panel.content, false);
 
    return panel;
}
