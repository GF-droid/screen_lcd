#include <string.h>
#include <time.h>
#include "page_conf.h"
#include "res_conf.h"

/* WiFi 连接条单实例: 全局引用, 供回调访问输入内容/图标 (set_init 只创建一次) */
static lv_obj_t* g_loading_img = NULL; /* 加载动画图标 */
static lv_obj_t* g_success_img = NULL; /* 连接成功对勾 */
static lv_obj_t* g_hint_label = NULL;  /* 失败提示文字 */
static lv_obj_t* g_wifi_kb = NULL;     /* 屏幕键盘 (连接时收起) */
static lv_timer_t* g_frame_timer = NULL;
static lv_timer_t* g_success_timer = NULL;
static lv_timer_t* g_hint_timer = NULL;
static wifi_connect_bar_t g_wifi_bar; /* 组件引用副本 (点击回调里取输入内容用) */

/* 顶栏动态元素: 时间 label + WiFi 图标/文案 (连接状态实时切换) */
static lv_obj_t* g_top_time_label = NULL;
static lv_obj_t* g_top_wifi_icon = NULL;
static lv_obj_t* g_top_wifi_label = NULL;
/* 上次顶栏 WiFi 状态 (状态没变不刷新). 初值 true: 顶栏创建时写死"已连接",
 * 真实状态未连接时首次轮询才会切到白色图标, 避免启动瞬间误跳 */
static bool g_top_wifi_connected = true;

/* wpa_manager 事件线程写, 主循环轮询读 (只写标志位, 线程安全) */
static volatile WPA_WIFI_CONNECT_STATUS_E g_conn_status = WPA_WIFI_INACTIVE;

/* 顶栏时钟回调 (定义在 top_component_add_info 之后) */
static void top_clock_timer_cb(lv_timer_t* t);

// 封装一个图标显示函数
static lv_obj_t* lv_image_vewer_create(lv_obj_t* parent, const char* image_path, lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, image_path);
    lv_obj_align(img, align, x_ofs, y_ofs);
    return img;
}

// 中间组件中的图标和文字显示
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

/* 输入框获得焦点时, 屏幕键盘切换到对应输入框并弹出.
   光标显示机制 (与 app_sdk 相同):
   1. lv_keyboard_set_textarea 会自动给新输入框加 LV_STATE_FOCUSED (并清掉旧的)
   2. 光标 = LV_PART_CURSOR | LV_STATE_FOCUSED 的 2px 竖线, 靠这个状态显示
   所以这里绝不能手动清 FOCUSED 状态, 否则光标永远不显示 (旧代码就是这么干的).
   也无需手动发 FOCUSED 事件 — 触摸点击 v9 的 click_focus 原生会发, 带了合法 indev 参数 */
static void wifi_input_focus_cb(lv_event_t* e) {
    lv_obj_t* ta = lv_event_get_target(e);    /* 刚获得焦点的输入框 */
    lv_obj_t* kb = lv_event_get_user_data(e); /* 屏幕键盘 */
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN); /* 弹出键盘 */
}

/* 点击键盘的 OK 确认键时, 收起键盘 */
static void wifi_kb_ready_cb(lv_event_t* e) {
    lv_obj_add_flag(lv_event_get_user_data(e), LV_OBJ_FLAG_HIDDEN);
}

// 注册中间矩形组件
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
    lv_label_set_text(label, "wifi连接");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, -10);

    /* WiFi 连接条: 账号输入 + 密码输入 + 连接按钮, 融入卡片中部偏下 */
    wifi_connect_bar_t wifi_bar = wifi_connect_bar_create(sp_panel, LV_PCT(100), 46);
    lv_obj_align(wifi_bar.bar, LV_ALIGN_BOTTOM_MID, 0, -20);

    /* 字体设在整条上, 输入框/按钮文案经继承链自动生效 (中文必需) */
    lv_font_t* font_bar = get_font(FONT_TYPE_CN, 14);
    if (font_bar != NULL) {
        lv_obj_set_style_text_font(wifi_bar.bar, font_bar, LV_STATE_DEFAULT);
    }

    /* 屏幕键盘: 模拟器上没有实体键盘, 用鼠标点键帽给输入框送字
       注意: 键盘放屏幕底部 (卡片只有 168px 高, 110px 的键盘放卡片里会盖住输入框) */
    lv_obj_t* kb = lv_keyboard_create(lv_screen_active());
    g_wifi_kb = kb;                        /* 供连接回调在开始时收起键盘 */
    lv_obj_set_size(kb, LV_PCT(100), 110); /* 模拟器窗口矮, 键盘压到 110px */
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, wifi_bar.ssid_input); /* 默认绑定账号框 (set_textarea 自带焦点管理) */
    /* v9.6 键盘构造时不清理 CLICK_FOCUSABLE (v8 会在构造里清), 键帽点击会把焦点从
       输入框抢走, 导致输入到一半光标就消失. 这里手动清掉, 与 app_sdk 行为一致 */
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); /* 初始隐藏, 点输入框才弹出 */

    /* ---------- 键盘样式: 深蓝风格 (T113 CPU 渲染, 刻意简化) ----------
     * 注意: 不用渐变/阴影/大圆角 —— T113 软件渲染这些每帧要几百 ms,
     * 40 个键帽会让帧率掉到 1fps. 纯色 + 小圆角渲染最快 */
    lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(kb, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(kb, 3, LV_STATE_DEFAULT);    /* 行间距 */
    lv_obj_set_style_pad_column(kb, 3, LV_STATE_DEFAULT); /* 列间距 */
    /* 键帽: 纯色深蓝 + 1px 描边 + 白字 */
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x2A3372), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(kb, 3, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(kb, lv_color_hex(0x4A54A0), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(kb, LV_OPA_60, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(kb, lv_color_white(), LV_PART_ITEMS | LV_STATE_DEFAULT);
    /* 键帽按下: 提亮 (纯色) */
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x4E57C0), LV_PART_ITEMS | LV_STATE_PRESSED);
    /* 功能键激活 (大写锁定/符号切换): 亮紫 */
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x6B7BFF), LV_PART_ITEMS | LV_STATE_CHECKED);

    /* 点击账号/密码框时, 键盘跟着切换绑定的输入框.
       触摸点击原生会发 FOCUSED (v9 click_focusable 默认开启), 光标/键盘都靠它 */
    lv_obj_add_event_cb(wifi_bar.ssid_input, wifi_input_focus_cb, LV_EVENT_CLICKED, kb);
    lv_obj_add_event_cb(wifi_bar.pwd_input, wifi_input_focus_cb, LV_EVENT_CLICKED, kb);
    lv_obj_add_event_cb(wifi_bar.ssid_input, wifi_input_focus_cb, LV_EVENT_FOCUSED, kb);
    lv_obj_add_event_cb(wifi_bar.pwd_input, wifi_input_focus_cb, LV_EVENT_FOCUSED, kb);
    /* 键盘 OK 键收起键盘 */
    lv_obj_add_event_cb(kb, wifi_kb_ready_cb, LV_EVENT_READY, kb);

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
        {IMAGE_PATH "iconfont_bule.png", "蓝牙:已连接"},
        {IMAGE_PATH "iconfont_yinl.png", "音量:50%"},
    };

    lv_font_t* font = get_font(FONT_TYPE_CN, 16); /* 顶栏文案字体 (中文必需) */
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        lv_obj_t* box = lv_obj_create(container);
        lv_obj_remove_style_all(box);
        lv_obj_set_size(box, LV_PCT(20), LV_PCT(100));

        lv_obj_t* icon = lv_image_vewer_create(box, items[i].icon_path, LV_ALIGN_LEFT_MID, 80, 0);
        lv_coord_t src_w = lv_image_get_src_width(icon); /* 原图是 200x200 大图, 按宽度缩到 16px */
        if (src_w > 0) {
            lv_image_set_scale(icon, 256 * 16 / src_w);
        }
        lv_obj_t* label = lv_label_create(box);
        lv_label_set_text(label, items[i].text);
        if (font != NULL) {
            lv_obj_set_style_text_font(label, font, LV_STATE_DEFAULT);
        }
        lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT); /* 默认黑色字在深色栏上看不见 */
        lv_obj_align_to(label, icon, LV_ALIGN_LEFT_MID, 30, 0);

        /* 动态元素: 记住指针, 供状态变化时更新 (时间实时刷新 / WiFi 图标切换) */
        if (strncmp(items[i].text, "时间:", 6) == 0) {
            g_top_time_label = label;
        } else if (strncmp(items[i].text, "WIFI:", 5) == 0) {
            g_top_wifi_icon = icon;
            g_top_wifi_label = label;
        }

        lv_obj_set_flex_grow(box, 1); /* 均分的关键 */
    }

    /* 顶栏时钟: 每秒刷新 "时间:HH:MM" */
    lv_timer_create(top_clock_timer_cb, 1000, NULL);
}

/* 顶栏时钟: 每秒更新时间显示 */
static void top_clock_timer_cb(lv_timer_t* t) {
    if (g_top_time_label == NULL)
        return;
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    char buf[24];
    snprintf(buf, sizeof(buf), "时间:%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
    lv_label_set_text(g_top_time_label, buf);
}

/* 顶栏 WiFi 图标/文案: 连接成功=蓝色图标, 未连接=白色图标 (状态没变不刷新) */
static void update_top_wifi_ui(void) {
    if (g_top_wifi_icon == NULL)
        return;
    bool conn = (g_conn_status == WPA_WIFI_CONNECT);
    if (conn == g_top_wifi_connected)
        return;
    g_top_wifi_connected = conn;
    lv_image_set_src(g_top_wifi_icon,
                     conn ? IMAGE_PATH "iconfont_wifi.png"       /* 蓝: 已连接 */
                          : IMAGE_PATH "iconfont_wifi_off.png"); /* 白: 未连接 */
    lv_label_set_text(g_top_wifi_label, conn ? "WIFI:已连接" : "WIFI:未连接");
}

/* ===== 加载动画 (用户图片的 8 帧旋转序列, 静态图切换, 无 transform 崩溃) ===== */
static const char* loading_frames[] = {
    IMAGE_PATH "iconfont_jiazai_0.png",
    IMAGE_PATH "iconfont_jiazai_1.png",
    IMAGE_PATH "iconfont_jiazai_2.png",
    IMAGE_PATH "iconfont_jiazai_3.png",
    IMAGE_PATH "iconfont_jiazai_4.png",
    IMAGE_PATH "iconfont_jiazai_5.png",
    IMAGE_PATH "iconfont_jiazai_6.png",
    IMAGE_PATH "iconfont_jiazai_7.png",
};
#define LOADING_FRAME_CNT 8
#define LOADING_FRAME_MS 100

/* ---------- 真实连接状态机 ----------
 * wpa_manager 在独立线程跑事件循环, 回调里不能碰 LVGL (非线程安全).
 * 线程回调只写 g_conn_status 标志 (定义见文件顶部), UI 更新由主循环 100ms 轮询完成. */
static bool g_connecting = false; /* 连接进行中 (防重复点击 + 超时判定) */
static uint32_t g_conn_start_tick = 0;
#define WIFI_CONN_TIMEOUT_MS 20000 /* 20s 无结果判定超时 */

/* 帧切换: 每 100ms 切下一帧 (模拟旋转) */
static void loading_frame_timer_cb(lv_timer_t* t) {
    static int idx = 0;
    idx = (idx + 1) % LOADING_FRAME_CNT;
    lv_image_set_src(g_loading_img, loading_frames[idx]);
}

/* 停止加载动画: 停帧定时器 + 隐藏图标 */
static void stop_loading(void) {
    if (g_frame_timer != NULL) {
        lv_timer_delete(g_frame_timer);
        g_frame_timer = NULL;
    }
    if (g_loading_img != NULL)
        lv_obj_add_flag(g_loading_img, LV_OBJ_FLAG_HIDDEN);
}

/* 开始加载动画 (同时清掉对勾/提示) */
static void start_loading(void) {
    stop_loading();
    if (g_success_img != NULL)
        lv_obj_add_flag(g_success_img, LV_OBJ_FLAG_HIDDEN);
    if (g_hint_label != NULL)
        lv_obj_add_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_loading_img, LV_OBJ_FLAG_HIDDEN);
    g_frame_timer = lv_timer_create(loading_frame_timer_cb, LOADING_FRAME_MS, NULL);
}

/* 连接成功: 停转 + 显示对勾 3 秒后隐藏 */
static void success_done_cb(lv_timer_t* t) {
    g_success_timer = NULL;
    lv_timer_delete(t);
    lv_obj_add_flag(g_success_img, LV_OBJ_FLAG_HIDDEN);
}

static void show_success(void) {
    stop_loading();
    if (g_hint_label != NULL)
        lv_obj_add_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
    if (g_success_timer != NULL)
        lv_timer_delete(g_success_timer); /* 防上次的残留定时器提前隐藏 */
    lv_obj_clear_flag(g_success_img, LV_OBJ_FLAG_HIDDEN);
    g_success_timer = lv_timer_create(success_done_cb, 3000, NULL);
    lv_timer_set_repeat_count(g_success_timer, 1);
}

/* 失败提示: 红字显示 3 秒后隐藏 (密码错误/超时/未填) */
static void hint_done_cb(lv_timer_t* t) {
    g_hint_timer = NULL;
    lv_timer_delete(t);
    if (g_hint_label != NULL)
        lv_obj_add_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
}

static void show_hint(const char* text) {
    stop_loading();
    if (g_success_img != NULL)
        lv_obj_add_flag(g_success_img, LV_OBJ_FLAG_HIDDEN);
    if (g_hint_timer != NULL)
        lv_timer_delete(g_hint_timer);
    lv_label_set_text(g_hint_label, text);
    lv_obj_clear_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
    g_hint_timer = lv_timer_create(hint_done_cb, 3000, NULL);
    lv_timer_set_repeat_count(g_hint_timer, 1);
}

/* wpa_manager 事件线程回调 → 只写标志位, 绝不操作 LVGL (非线程安全) */
void wifi_status_ui_cb(WPA_WIFI_CONNECT_STATUS_E status) {
    g_conn_status = status;
}

/* 主循环轮询: 检测连接结果/超时 → 更新 UI (LVGL 主线程, 安全) */
static void wifi_conn_poll_cb(lv_timer_t* t) {
    /* 顶栏 WiFi 状态: 每次轮询都检查 (启动时反映真实状态, 连接/断开随时切换) */
    update_top_wifi_ui();

    if (!g_connecting)
        return;

    /* 超时: 20s 内没有 CONNECT/WRONG_KEY 结果 */
    if (lv_tick_get() - g_conn_start_tick > WIFI_CONN_TIMEOUT_MS) {
        g_connecting = false;
        show_hint("连接超时");
        return;
    }

    switch (g_conn_status) {
        case WPA_WIFI_CONNECT:
            g_connecting = false;
            show_success();
            break;
        case WPA_WIFI_WRONG_KEY:
            g_connecting = false;
            show_hint("密码错误");
            break;
        default:
            /* INACTIVE/SCANNING/DISCONNECT: 连接过程中的正常事件, 继续等 */
            break;
    }
}

/* 连接按钮点击: 校验输入 → 起动画 → 调硬件层连接 */
static void connect_btn_click_cb(lv_event_t* e) {
    if (g_connecting)
        return; /* 连接中, 防重复点击 */

    const char* ssid = lv_textarea_get_text(g_wifi_bar.ssid_input);
    const char* psw = lv_textarea_get_text(g_wifi_bar.pwd_input);
    if (ssid == NULL || psw == NULL || ssid[0] == '\0' || psw[0] == '\0') {
        show_hint("请输入账号密码");
        return;
    }

    /* 收起键盘, 进入连接流程 */
    if (g_wifi_kb != NULL)
        lv_obj_add_flag(g_wifi_kb, LV_OBJ_FLAG_HIDDEN);
    g_connecting = true;
    g_conn_status = WPA_WIFI_INACTIVE; /* 重置, 等 wpa 事件刷新 */
    g_conn_start_tick = lv_tick_get();
    start_loading();

    wpa_ctrl_wifi_info_t info = {0};
    strncpy(info.ssid, ssid, sizeof(info.ssid) - 1);
    strncpy(info.psw, psw, sizeof(info.psw) - 1);
    printf("[wifi] connect \"%s\"\n", info.ssid);
    wpa_manager_wifi_connect(&info);
}

wifi_connect_bar_t wifi_connect_bar_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
    wifi_connect_bar_style_init();

    wifi_connect_bar_t ui = {0};

    /* 1. 整条背景 */
    ui.bar = lv_obj_create(parent);
    lv_obj_remove_style_all(ui.bar);
    lv_obj_add_style(ui.bar, &style_bar, LV_STATE_DEFAULT);
    lv_obj_set_size(ui.bar, w, h);
    lv_obj_set_scrollable(ui.bar, false);

    /* 2. WiFi 账号输入框：宽度权重 2 */
    ui.ssid_input = lv_textarea_create(ui.bar);
    lv_obj_remove_style_all(ui.ssid_input);
    lv_obj_add_style(ui.ssid_input, &style_input, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui.ssid_input, lv_color_white(), LV_PART_TEXTAREA_PLACEHOLDER); /* 占位符白色, 更醒目 */
    /* 光标: remove_style_all 连主题的 ta_cursor 样式(2px 竖线)也删了, 自己补上.
       白色 2px 左竖线 + 400ms 闪烁, 聚焦 (LV_STATE_FOCUSED) 才显示 — 和 app_sdk 效果一致 */
    lv_obj_set_style_border_width(ui.ssid_input, 2, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ui.ssid_input, lv_color_white(), LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_side(ui.ssid_input, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_pad_left(ui.ssid_input, -1, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_anim_duration(ui.ssid_input, 400, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_textarea_set_one_line(ui.ssid_input, true);
    lv_textarea_set_placeholder_text(ui.ssid_input, "WiFi 账号");
    lv_obj_set_flex_grow(ui.ssid_input, 2);

    /* 3. 密码输入框：宽度权重 2，开启密码遮挡 */
    ui.pwd_input = lv_textarea_create(ui.bar);
    lv_obj_remove_style_all(ui.pwd_input);
    lv_obj_add_style(ui.pwd_input, &style_input, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui.pwd_input, lv_color_white(), LV_PART_TEXTAREA_PLACEHOLDER);
    /* 光标样式同账号框 (见上方注释) */
    lv_obj_set_style_border_width(ui.pwd_input, 2, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ui.pwd_input, lv_color_white(), LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_side(ui.pwd_input, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_pad_left(ui.pwd_input, -1, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_anim_duration(ui.pwd_input, 400, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_textarea_set_one_line(ui.pwd_input, true);
    lv_textarea_set_password_mode(ui.pwd_input, true);
    lv_textarea_set_placeholder_text(ui.pwd_input, "密码");
    lv_obj_set_flex_grow(ui.pwd_input, 2);

    /* 4. 连接按钮：宽度权重 1（比两个输入框窄一些）, 高度填满条 (flex 默认按内容高太扁) */
    ui.connect_btn = lv_btn_create(ui.bar);
    lv_obj_remove_style_all(ui.connect_btn);
    lv_obj_add_style(ui.connect_btn, &style_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(ui.connect_btn, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_flex_grow(ui.connect_btn, 1);
    lv_obj_set_height(ui.connect_btn, LV_PCT(100)); /* 填满条内容区高度 */
    lv_obj_set_scrollable(ui.connect_btn, false);

    lv_obj_t* btn_label = lv_label_create(ui.connect_btn);
    lv_obj_add_style(btn_label, &style_btn_label, LV_STATE_DEFAULT);
    lv_label_set_text(btn_label, "连接");
    lv_obj_center(btn_label);

    /* 5. 加载动画: 用户图片的 8 帧旋转序列, 点击"连接"后才转
       注意: 帧序列 = 静态图切换 (lv_image_set_src), 无 transform, T113 上安全 */
    ui.loading_img = lv_image_create(ui.bar);
    lv_image_set_src(ui.loading_img, loading_frames[0]);
    lv_obj_set_size(ui.loading_img, 24, 24);
    lv_obj_add_flag(ui.loading_img, LV_OBJ_FLAG_HIDDEN); /* 初始隐藏 */
    g_loading_img = ui.loading_img;

    /* 6. 连接成功图标: 对勾, 加载完成后显示 */
    ui.success_img = lv_image_create(ui.bar);
    lv_image_set_src(ui.success_img, IMAGE_PATH "iconfont_zhenque.png");
    lv_obj_set_size(ui.success_img, 28, 28);
    lv_obj_add_flag(ui.success_img, LV_OBJ_FLAG_HIDDEN);
    g_success_img = ui.success_img;

    /* 7. 失败提示文字: 密码错误/连接超时等, 红色, 默认隐藏 */
    ui.hint_label = lv_label_create(ui.bar);
    lv_obj_set_style_text_color(ui.hint_label, lv_color_hex(0xFF6B6B), LV_STATE_DEFAULT);
    lv_label_set_text(ui.hint_label, "");
    lv_obj_add_flag(ui.hint_label, LV_OBJ_FLAG_HIDDEN);
    g_hint_label = ui.hint_label;

    /* 连接按钮点击 → 真实 WiFi 连接 (加载动画跟随连接流程) */
    lv_obj_add_event_cb(ui.connect_btn, connect_btn_click_cb, LV_EVENT_CLICKED, NULL);

    /* 连接结果轮询: 100ms 查一次 wpa 线程写下的状态标志 (UI 更新只在主循环, 线程安全) */
    lv_timer_create(wifi_conn_poll_cb, 100, NULL);

    g_wifi_bar = ui; /* 保存引用, 供点击回调读取输入内容 */
    return ui;
}

void set_init(void) {
    font_init(); /* 注册 TTF 字体路径, 必须在任何 get_font() 之前 */

    lv_image_vewer_create(lv_screen_active(), IMAGE_PATH "back.png", LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t* top_box = top_component_create(lv_screen_active()); /* 第一部分: 盒子 */
    top_component_add_info(top_box);                              /* 第二部分: 显示信息 */

    lv_obj_t* bottom_img = bottom_bar_create(lv_screen_active()); /* 程序化底部栏, 替换 bottom.png */

    lv_obj_t* panel = settings_panel_create(lv_scr_act(), LV_PCT(20), LV_PCT(65));
    lv_obj_align_to(panel, lv_scr_act(), LV_ALIGN_LEFT_MID, 107, 0);
    lv_obj_update_layout(panel);

    lv_font_t* font = get_font(FONT_TYPE_CN, 10);
    if (font != NULL) {
        lv_obj_set_style_text_font(panel, font, LV_STATE_DEFAULT);
    }
    /* TODO: 把你的图标 PNG 放到 res/image/ 下, 把 NULL 换成 IMAGE_PATH "你的图标.png" */
    settings_row_t row_bt = row_add_clickable(panel, IMAGE_PATH "iconfont_bule.png", "蓝牙设置", "显示与亮度");
    settings_row_t row_disp = row_add_clickable(panel, IMAGE_PATH "wangl.png", "网络连接 ", "wifi连接");
    settings_row_t row_update = row_add_clickable(panel, IMAGE_PATH "iconfont_genx.png", "网络设置", "蓝牙设置");
    settings_row_t row_alarm = row_add_clickable(panel, IMAGE_PATH "iconfont_laoz.png", "闹钟管理", "闹钟管理");
    settings_row_t row_log = row_add_clickable(panel, IMAGE_PATH "iconfont_riz.png", "日志记录", "系统更新");

    settings_row_set_selected(&row_alarm, true); /* 初始选中第 3 行 */

    lv_obj_t* sp_panel = lv_mid_screen_componnet_create(lv_screen_active());
    lv_obj_align_to(sp_panel, panel, LV_ALIGN_OUT_RIGHT_MID, 66, 0);

    lv_obj_t* btn = bottom_bar_btn_create(bottom_img, "返回"); /* 深蓝玻璃, 与底部栏同色系 */

    /* 字体设在按钮对象上, 经继承链传给内部文案 label */
    lv_font_t* font_btn = get_font(FONT_TYPE_CN, 30);
    if (font_btn != NULL) {
        lv_obj_set_style_text_font(btn, font_btn, LV_STATE_DEFAULT);
    }
}
