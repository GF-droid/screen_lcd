#include "page_conf.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>

#include "widgets/widget_wifi.h"
#include "widgets/widget_common.h"
#include "setting.h"

/* ==================== WiFi 连接组件 (demo2 移植) ==================== */
static lv_obj_t* g_loading_img = NULL; /* 加载动画图标 */

static lv_obj_t* g_success_img = NULL; /* 连接成功对勾 */
static lv_obj_t* g_hint_label = NULL;  /* 失败提示文字 */
static lv_obj_t* g_wifi_kb = NULL;     /* 屏幕键盘 (连接时/切换页面时收起) */

/* 切换组件时收起屏幕键盘 (setting.c 调用) */
void wifi_kb_hide(void) {
    if (g_wifi_kb != NULL)
        lv_obj_add_flag(g_wifi_kb, LV_OBJ_FLAG_HIDDEN);
}
static lv_timer_t* g_frame_timer = NULL;
static lv_timer_t* g_success_timer = NULL;
static lv_timer_t* g_hint_timer = NULL;
static wifi_connect_bar_t g_wifi_bar; /* 组件引用副本 (点击回调里取输入内容用) */

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

/* WiFi 组件容器: 标题 + WiFi 连接条 + 屏幕键盘 (键盘在 screen 上, 切换页面时单独收起) */
lv_obj_t* wifi_component_create(lv_obj_t* parent) {
    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(root, false);

    /* 标题: 30px 白字 "wifi连接", 挂 root 上 (随组件一起隐藏, 不会与别的组件标题叠加).
       root 在卡片内容区顶部, TOP_MID(0,0) 标题正好位于卡片上方内容区 */
    lv_obj_t* label = lv_label_create(root);
    lv_font_t* font = get_font(FONT_TYPE_CN, 30);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, LV_STATE_DEFAULT);
    }
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(label, "wifi连接");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

    /* WiFi 连接条: 账号输入 + 密码输入 + 连接按钮, 融入卡片中部偏下 */
    wifi_connect_bar_t wifi_bar = wifi_connect_bar_create(root, LV_PCT(100), 46);
    lv_obj_align(wifi_bar.bar, LV_ALIGN_BOTTOM_MID, 0, -20);

    /* 字体设在整条上, 输入框/按钮文案经继承链自动生效 (中文必需) */
    lv_font_t* font_bar = get_font(FONT_TYPE_CN, 14);
    if (font_bar != NULL) {
        lv_obj_set_style_text_font(wifi_bar.bar, font_bar, LV_STATE_DEFAULT);
    }

    /* 屏幕键盘: 模拟器上没有实体键盘, 用鼠标点键帽给输入框送字
       注意: 键盘放屏幕底部 (卡片只有 168px 高, 110px 的键盘放卡片里会盖住输入框) */
    lv_obj_t* kb = lv_keyboard_create(g_setting_screen);
    g_wifi_kb = kb;                        /* 供连接回调/页面切换时收起键盘 */
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

    return root;
}


static bool wifi_bar_style_inited = false;

void wifi_connect_bar_style_init(void) {
    if (wifi_bar_style_inited)
        return;
    wifi_bar_style_inited = true;

    /* ---------- 整条背景: 半透明深蓝底, 内部 flex 横向排 [输入框][输入框][按钮] ---------- */
    lv_style_init(&style_bar);
    lv_style_set_radius(&style_bar, 14);
    lv_style_set_bg_opa(&style_bar, LV_OPA_60);
    lv_style_set_bg_color(&style_bar, lv_color_hex(0x1A2150));
    lv_style_set_border_width(&style_bar, 1);
    lv_style_set_border_color(&style_bar, lv_color_hex(0x9DB4FF));
    lv_style_set_border_opa(&style_bar, LV_OPA_30);
    lv_style_set_pad_all(&style_bar, 6);
    lv_style_set_pad_column(&style_bar, 8); /* 三个子元素的间距 */
    lv_style_set_layout(&style_bar, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&style_bar, LV_FLEX_FLOW_ROW);
    lv_style_set_flex_cross_place(&style_bar, LV_FLEX_ALIGN_CENTER); /* 垂直居中 */

    /* ---------- 输入框: 深色底 + 亮蓝描边 (增强可见性), 文字白色 (字体从父链继承) ---------- */
    lv_style_init(&style_input);
    lv_style_set_radius(&style_input, 8);
    lv_style_set_bg_opa(&style_input, LV_OPA_90);
    lv_style_set_bg_color(&style_input, lv_color_hex(0x101A3E)); /* 比卡片底稍亮, 输入框明显 */
    lv_style_set_border_width(&style_input, 2);                  /* 1→2px, 更醒目 */
    lv_style_set_border_color(&style_input, lv_color_hex(0xB8C8FF));
    lv_style_set_border_opa(&style_input, LV_OPA_70);            /* 30→70% */
    lv_style_set_text_color(&style_input, lv_color_white());
    lv_style_set_pad_left(&style_input, 10);
    lv_style_set_pad_right(&style_input, 10);

    /* ---------- 连接按钮: 和气泡胶囊同风格的玻璃渐变 + 微光晕 ---------- */
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 8);
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn, lv_color_hex(0x6B7BFF));      /* 按钮-顶部 */
    lv_style_set_bg_grad_color(&style_btn, lv_color_hex(0x3A46B8)); /* 按钮-底部 */
    lv_style_set_bg_grad_dir(&style_btn, LV_GRAD_DIR_VER);
    lv_style_set_shadow_width(&style_btn, 8);
    lv_style_set_shadow_color(&style_btn, lv_color_hex(0x7C86FF));
    lv_style_set_shadow_opa(&style_btn, LV_OPA_30);

    /* ---------- 按下态: 整体加深 ---------- */
    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, lv_color_hex(0x4E57C0));
    lv_style_set_bg_grad_color(&style_btn_pressed, lv_color_hex(0x2E3690));

    /* ---------- 按钮文案 (字体从父链继承) ---------- */
    lv_style_init(&style_btn_label);
    lv_style_set_text_color(&style_btn_label, lv_color_white());
}

/* -------------------- 蓝牙/亮度/音量 slider 样式 (定义+声明见 widget_dev.c) -------------------- */
