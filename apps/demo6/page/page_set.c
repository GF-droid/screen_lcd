#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>
#include "page_conf.h"
#include "res_conf.h"

/* ============ 公共: 顶栏动态元素 ============ */
static lv_obj_t* g_top_time_label = NULL;
static lv_obj_t* g_top_wifi_icon = NULL;
static lv_obj_t* g_top_wifi_label = NULL;
static lv_obj_t* g_top_vol_label = NULL; /* 音量: 右栏 slider 变化时联动刷新 */
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
        } else if (strncmp(items[i].text, "音量:", 6) == 0) {
            g_top_vol_label = label; /* 音量: 右栏 slider 变化时联动刷新 */
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

/* 顶栏 WiFi 图标/文案: 连接成功=蓝色图标, 未连接=白色图标 (状态没变不刷新)
 * 返回状态是否发生变化, 变化时由 sys_timer_cb 记录日志 */
static bool update_top_wifi_ui(void) {
    if (g_top_wifi_icon == NULL)
        return false;
    bool conn = (g_conn_status == WPA_WIFI_CONNECT);
    if (conn == g_top_wifi_connected)
        return false;
    g_top_wifi_connected = conn;
    lv_image_set_src(g_top_wifi_icon,
                     conn ? IMAGE_PATH "iconfont_wifi.png"       /* 蓝: 已连接 */
                          : IMAGE_PATH "iconfont_wifi_off.png"); /* 白: 未连接 */
    lv_label_set_text(g_top_wifi_label, conn ? "WIFI:已连接" : "WIFI:未连接");
    return true;
}

/* wpa_manager 事件线程回调 → 只写标志位, 绝不操作 LVGL (非线程安全) */
void wifi_status_ui_cb(WPA_WIFI_CONNECT_STATUS_E status) {
    g_conn_status = status;
}

/* ==================== WiFi 连接组件 (demo2 移植) ==================== */
static lv_obj_t* g_loading_img = NULL; /* 加载动画图标 */
static lv_obj_t* g_success_img = NULL; /* 连接成功对勾 */
static lv_obj_t* g_hint_label = NULL;  /* 失败提示文字 */
static lv_obj_t* g_wifi_kb = NULL;     /* 屏幕键盘 (连接时/切换页面时收起) */
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
static lv_obj_t* wifi_component_create(lv_obj_t* parent) {
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
    lv_obj_t* kb = lv_keyboard_create(lv_screen_active());
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

/* ==================== 网络信息气泡组件 (demo1 移植: 气泡展示网络相关信息) ==================== */

/* 8 个气泡的文案 label, 由定时器周期刷新为真实数据 (WiFi 状态/IP/掩码) */
static lv_obj_t* g_net_chip_labels[8] = {0};

/* 取活动网卡 (非 lo 且 UP) 的 IPv4 地址+掩码; 找不到返回 false */
static bool get_net_addr(char* ip, size_t ip_sz, char* mask, size_t mask_sz) {
    struct ifaddrs* ifa0 = NULL;
    if (getifaddrs(&ifa0) != 0)
        return false;
    bool found = false;
    for (struct ifaddrs* ifa = ifa0; ifa != NULL && !found; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (strcmp(ifa->ifa_name, "lo") == 0)
            continue;
        if ((ifa->ifa_flags & IFF_UP) == 0)
            continue;
        struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
        struct sockaddr_in* sm = (struct sockaddr_in*)ifa->ifa_netmask;
        inet_ntop(AF_INET, &sa->sin_addr, ip, ip_sz);
        if (sm != NULL && sm->sin_addr.s_addr != 0)
            inet_ntop(AF_INET, &sm->sin_addr, mask, mask_sz);
        else
            snprintf(mask, mask_sz, "255.255.255.0"); /* 掩码缺失兜底 */
        found = true;
    }
    freeifaddrs(ifa0);
    return found;
}

/* WiFi 连接状态 → 气泡文案 (g_conn_status 由 wpa 事件线程写, 主循环轮询读) */
static const char* wifi_status_text(WPA_WIFI_CONNECT_STATUS_E s) {
    switch (s) {
        case WPA_WIFI_CONNECT:    return "WIFI已连接";
        case WPA_WIFI_DISCONNECT: return "WIFI未连接";
        case WPA_WIFI_WRONG_KEY:  return "WIFI密码错误";
        case WPA_WIFI_SCANNING:   return "WIFI扫描中";
        default:                  return "WIFI未启动"; /* INACTIVE */
    }
}

/* 刷新一次真实数据 (创建时 + 定时器周期各调一次) */
static void net_info_refresh(void) {
    if (g_net_chip_labels[0] != NULL)
        lv_label_set_text(g_net_chip_labels[0], wifi_status_text(g_conn_status));

    char ip[INET_ADDRSTRLEN] = {0}, mask[INET_ADDRSTRLEN] = {0};
    char buf[64];
    if (get_net_addr(ip, sizeof(ip), mask, sizeof(mask))) {
        if (g_net_chip_labels[2] != NULL) {
            snprintf(buf, sizeof(buf), "IP:%s", ip);
            lv_label_set_text(g_net_chip_labels[2], buf);
        }
        if (g_net_chip_labels[4] != NULL) {
            snprintf(buf, sizeof(buf), "掩码:%s", mask);
            lv_label_set_text(g_net_chip_labels[4], buf);
        }
    }
}

/* 周期刷新 (1s): WiFi 状态/IP/掩码 跟随真实状态 */
static void net_info_refresh_cb(lv_timer_t* t) {
    (void)t;
    net_info_refresh();
}

/* 气泡 + 图标: 图标缩到 16px 宽放气泡左侧 (与 demo1 lv_image_vewer_create_with_size 一致).
   label_out 非空时返回文案 label 指针 (child[1]), 供周期刷新真实数据用 */
static lv_obj_t* net_chip_with_icon(lv_obj_t* parent, const char* icon_path, lv_coord_t w, lv_coord_t h, const char* text, lv_obj_t** label_out) {
    lv_obj_t* chip = chip_bubble_create(parent, w, h, text);
    if (label_out != NULL)
        *label_out = lv_obj_get_child(chip, 1); /* child[0] 图标槽, child[1] 文案 label */
    lv_obj_t* img = lv_image_create(chip);
    lv_image_set_src(img, icon_path);
    lv_coord_t src_w = lv_image_get_src_width(img); /* 原图宽度 */
    if (src_w > 0) {
        lv_image_set_scale(img, 256 * 16 / src_w); /* 按宽度缩到 16px, 高度等比 */
    }
    lv_obj_set_ignore_layout(img, true); /* 不参与 flex 布局, 由手动 align 定位 */
    lv_obj_align_to(img, chip, LV_ALIGN_LEFT_MID, -5, 0);

    return chip;
}

/* 网络信息组件: 标题 + 深色底板 (flex 换行) + 8 个信息气泡 (demo1 布局) */
static lv_obj_t* net_info_component_create(lv_obj_t* parent) {
    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), 172); /* 高 172: 覆盖气泡第二行底 228 (root 顶=卡片 y56) */
    lv_obj_set_scrollable(root, false);

    /* 标题: 30px 白字 "状态卡片", 挂 root 上 (随组件一起隐藏, 不会与别的组件标题叠加) */
    lv_obj_t* label = lv_label_create(root);
    lv_font_t* font = get_font(FONT_TYPE_CN, 30);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, LV_STATE_DEFAULT);
    }
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(label, "状态卡片");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

    /* 深色内容底板: 手动对齐.
       LVGL v9.6: 子对象 pos(0,0) 和 align 的基准是父的内容区起点 (父原点 + pad 24)!
       root 在内容区 (481,80). 与 demo1 几何一致: temp 高 94 (LV_PCT(80) 取整),
       BOTTOM_MID +15 → 绝对底 215, 上缘 121 → 相对 root: y = 121-80 = 41.
       x 必须为 0: root 已在内容区 x481, 再偏移会把一行 5 个气泡 (800px) 顶出
       内容区右边界 x1287 (481+24+806=1287 是根底板的右缘), 最后气泡被截断 */
    lv_obj_t* temp = lv_obj_create(root);
    lv_obj_remove_style_all(temp);
    lv_obj_set_size(temp, LV_PCT(100), 94);
    lv_obj_align(temp, LV_ALIGN_TOP_LEFT, 0, 41);
    lv_obj_set_clickable(temp, false);
    lv_obj_set_scrollable(temp, false);
    /* flex 自动换行: 从左往右排, 一行放不下自动换下一行, 间距 25px (与 demo1 一致) */
    lv_obj_set_style_layout(temp, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(temp, LV_FLEX_FLOW_ROW_WRAP, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(temp, 25, LV_STATE_DEFAULT); /* 列间距 */
    lv_obj_set_style_pad_row(temp, 25, LV_STATE_DEFAULT);    /* 行间距 */

    lv_font_t* fontback = get_font(FONT_TYPE_CN, 10);
    if (fontback != NULL) {
        lv_obj_set_style_text_font(root, fontback, LV_STATE_DEFAULT);
    }

    /* 气泡由 flex 自动排列成行: 140px 宽, 一行 5 个, 8 个换第二行 (demo1 同款).
       带真实数据的三个气泡导出 label 指针: 0=WiFi状态, 2=IP, 4=掩码 */
    net_chip_with_icon(temp, IMAGE_PATH "iconfont_wifi.png", 140, 30, "WIFI办公室", &g_net_chip_labels[0]);
    net_chip_with_icon(temp, IMAGE_PATH "lany.png", 140, 30, "蓝牙办公室", NULL);
    net_chip_with_icon(temp, IMAGE_PATH "iconfont_ip.png", 140, 30, "IP地址:127.0.0.1", &g_net_chip_labels[2]);
    net_chip_with_icon(temp, IMAGE_PATH "ceshi.png", 140, 30, "网络测试", NULL);
    net_chip_with_icon(temp, IMAGE_PATH "xianshima.png", 140, 30, "子掩饰码", &g_net_chip_labels[4]);
    net_chip_with_icon(temp, IMAGE_PATH "iconfont_guan.png", 140, 30, "待定", NULL); /* demo1 无此图, 图标区空白, 保持一致 */
    net_chip_with_icon(temp, IMAGE_PATH "iconfont_bule.png", 140, 30, "待定", NULL);
    net_chip_with_icon(temp, IMAGE_PATH "iconfont_guan.png", 140, 30, "待定", NULL);

    /* 创建即刷一次真实值 (定时器首跳前不显示旧占位文本), 之后 1s 周期跟随 */
    net_info_refresh();
    lv_timer_create(net_info_refresh_cb, 1000, NULL);

    return root;
}

/* ==================== 闹钟管理组件 (demo3 移植: 左半闹钟设置, 右半倒计时) ==================== */

/* 响铃: 板上 aplay 后台播放 (shell 的 & 立即返回, 不卡 UI); 模拟器跳过并打印 */
static void alarm_play_sound(const char* path) {
#ifdef SIMULATOR_LINUX
    printf("[alarm] 模拟器跳过播放: %s\n", path);
    fflush(stdout);
#else
    char cmd[192];
    snprintf(cmd, sizeof(cmd), "killall aplay 2>/dev/null; aplay %s &", path);
    system(cmd);
#endif
}

/* 生成 "00\n01\n...\n<max>" roller 选项串, 索引即数值 (index==value) */
static void build_roller_options(char* buf, int max_val) {
    char* p = buf;
    for (int i = 0; i <= max_val; i++)
        p += sprintf(p, "%s%02d", i ? "\n" : "", i);
}

/* ---------- 闹钟设置 (左栏) ---------- */
static lv_obj_t* g_alarm_big_label = NULL;  /* 大字 "HH:MM" */
static lv_obj_t* g_alarm_switch = NULL;     /* 开/关胶囊按钮 */
static lv_obj_t* g_alarm_roller_h = NULL;   /* 小时 roller (00-23) */
static lv_obj_t* g_alarm_roller_m = NULL;   /* 分钟 roller (00-59) */
static bool g_alarm_fired = false;          /* 本周期是否已响过铃 (edge 触发) */

/* roller 变更 → 刷新大字时间 + 允许重新到点响铃 */
static void alarm_roller_change_cb(lv_event_t* e) {
    int h = (int)lv_roller_get_selected(g_alarm_roller_h);
    int m = (int)lv_roller_get_selected(g_alarm_roller_m);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    lv_label_set_text(g_alarm_big_label, buf);
    g_alarm_fired = false;
}

/* 开关点击: CHECKED 态取反, 文案 开/关 同步 */
static void alarm_switch_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    if (lv_obj_has_state(btn, LV_STATE_CHECKED))
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    else
        lv_obj_add_state(btn, LV_STATE_CHECKED);
    lv_obj_t* label = lv_event_get_user_data(e);
    lv_label_set_text(label, lv_obj_has_state(btn, LV_STATE_CHECKED) ? "开" : "关");
}

/* 创建左栏: 大字时间 + 开关 + 时/分 roller 横条 */
static lv_obj_t* alarm_col_left_create(lv_obj_t* parent) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(col, false);

    /* 大字时间 (40px 中文 TTF, 数字高度充裕) */
    g_alarm_big_label = lv_label_create(col);
    lv_font_t* font_big = get_font(FONT_TYPE_CN, 40);
    if (font_big != NULL)
        lv_obj_set_style_text_font(g_alarm_big_label, font_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_alarm_big_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(g_alarm_big_label, "07:00");
    lv_obj_align(g_alarm_big_label, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 开关胶囊: 绝对定位右下角, 与右栏按钮行同处底部 */
    lv_obj_t* sw = lv_btn_create(col);
    lv_obj_remove_style_all(sw);
    lv_obj_add_style(sw, &style_alarm_switch, LV_STATE_DEFAULT);
    lv_obj_add_style(sw, &style_alarm_switch_checked, LV_STATE_CHECKED);
    lv_obj_set_size(sw, 64, 24);
    lv_obj_align(sw, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_scrollable(sw, false);
    lv_obj_t* sw_label = lv_label_create(sw);
    lv_font_t* font_sw = get_font(FONT_TYPE_CN, 14);
    if (font_sw != NULL)
        lv_obj_set_style_text_font(sw_label, font_sw, LV_STATE_DEFAULT);
    lv_label_set_text(sw_label, "关");
    lv_obj_center(sw_label);
    lv_obj_add_event_cb(sw, alarm_switch_cb, LV_EVENT_CLICKED, sw_label);
    g_alarm_switch = sw;

    /* 时:分 roller 横条 (flex row 居中) */
    lv_obj_t* bar = lv_obj_create(col);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 142, 57);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_layout(bar, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(bar, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(bar, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    char opts[256];
    build_roller_options(opts, 23);
    g_alarm_roller_h = lv_roller_create(bar);
    lv_obj_remove_style_all(g_alarm_roller_h);
    lv_obj_add_style(g_alarm_roller_h, &style_alarm_roller, LV_STATE_DEFAULT);
    lv_obj_add_style(g_alarm_roller_h, &style_alarm_roller_sel, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_font_t* font_r = get_font(FONT_TYPE_CN, 14); /* 必须先设字体, set_visible_row_count 按字体算高 */
    if (font_r != NULL)
        lv_obj_set_style_text_font(g_alarm_roller_h, font_r, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_roller_set_options(g_alarm_roller_h, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_alarm_roller_h, 3);
    lv_roller_set_selected(g_alarm_roller_h, 7, LV_ANIM_OFF); /* 默认 07 */
    lv_obj_set_width(g_alarm_roller_h, 64);
    lv_obj_set_style_text_align(g_alarm_roller_h, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_alarm_roller_h, alarm_roller_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* colon = lv_label_create(bar);
    lv_font_t* font_c = get_font(FONT_TYPE_CN, 14);
    if (font_c != NULL)
        lv_obj_set_style_text_font(colon, font_c, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(colon, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_pad_left(colon, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(colon, 2, LV_STATE_DEFAULT);

    build_roller_options(opts, 59);
    g_alarm_roller_m = lv_roller_create(bar);
    lv_obj_remove_style_all(g_alarm_roller_m);
    lv_obj_add_style(g_alarm_roller_m, &style_alarm_roller, LV_STATE_DEFAULT);
    lv_obj_add_style(g_alarm_roller_m, &style_alarm_roller_sel, LV_PART_SELECTED | LV_STATE_DEFAULT);
    if (font_r != NULL)
        lv_obj_set_style_text_font(g_alarm_roller_m, font_r, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_roller_set_options(g_alarm_roller_m, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_alarm_roller_m, 3);
    lv_roller_set_selected(g_alarm_roller_m, 0, LV_ANIM_OFF); /* 默认 00 */
    lv_obj_set_width(g_alarm_roller_m, 64);
    lv_obj_set_style_text_align(g_alarm_roller_m, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_alarm_roller_m, alarm_roller_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return col;
}

/* ---------- 倒计时 (右栏) ---------- */
typedef enum { CD_IDLE, CD_RUNNING, CD_PAUSED } cd_state_t;

static lv_obj_t* g_cd_label = NULL;          /* 大字 "MM:SS" */
static lv_obj_t* g_cd_roller_m = NULL;       /* 分钟 roller (00-59) */
static lv_obj_t* g_cd_roller_s = NULL;       /* 秒 roller (00-59) */
static lv_obj_t* g_cd_btn_start = NULL;      /* 开始/继续 */
static lv_obj_t* g_cd_btn_pause = NULL;      /* 暂停 */
static lv_obj_t* g_cd_btn_reset = NULL;      /* 重置 */
static lv_obj_t* g_cd_btn_start_label = NULL;
static lv_obj_t* g_cd_btn_pause_label = NULL;
static cd_state_t g_cd_state = CD_IDLE;
static int32_t g_cd_total = 0;      /* 设定总秒数 (重置时恢复) */
static int32_t g_cd_remaining = 0;  /* 剩余秒数 */

/* 倒计时 roller 变更 → 大字实时预览设定值 (仅 IDLE 可调, 运行中 roller 已禁用) */
static void cd_roller_change_cb(lv_event_t* e) {
    (void)e;
    int m = (int)lv_roller_get_selected(g_cd_roller_m);
    int s = (int)lv_roller_get_selected(g_cd_roller_s);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    lv_label_set_text(g_cd_label, buf);
}

/* 状态切换后统一刷新: 大字 label + 三个按钮的禁用态/文案 */
static void cd_update_ui(void) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", (int)(g_cd_remaining / 60), (int)(g_cd_remaining % 60));
    lv_label_set_text(g_cd_label, buf);

    bool idle = (g_cd_state == CD_IDLE);
    bool running = (g_cd_state == CD_RUNNING);
    lv_obj_t* m = g_cd_roller_m;
    lv_obj_t* s = g_cd_roller_s;

    /* 开始/继续: IDLE 可用, RUNNING/PAUSED 禁用 */
    lv_obj_set_state(g_cd_btn_start, LV_STATE_DISABLED, running);
    lv_label_set_text(g_cd_btn_start_label, (g_cd_state == CD_PAUSED) ? "继续" : "开始");
    /* 暂停: 仅 RUNNING 可用 */
    lv_obj_set_state(g_cd_btn_pause, LV_STATE_DISABLED, !running);
    lv_label_set_text(g_cd_btn_pause_label, (g_cd_state == CD_PAUSED) ? "已暂停" : "暂停");
    /* 重置: IDLE 也可用 (回到设定时长) */
    lv_obj_set_state(g_cd_btn_reset, LV_STATE_DISABLED, false);
    /* roller: 仅 IDLE 可调 */
    lv_obj_set_state(m, LV_STATE_DISABLED, !idle);
    lv_obj_set_state(s, LV_STATE_DISABLED, !idle);
}

/* 开始/继续 */
static void cd_btn_start_cb(lv_event_t* e) {
    if (g_cd_state == CD_RUNNING)
        return;
    if (g_cd_state == CD_IDLE) {
        int m = (int)lv_roller_get_selected(g_cd_roller_m);
        int s = (int)lv_roller_get_selected(g_cd_roller_s);
        g_cd_total = m * 60 + s;
        if (g_cd_total <= 0)
            return; /* 0 秒不允许开始 */
        g_cd_remaining = g_cd_total;
    }
    g_cd_state = CD_RUNNING;
    cd_update_ui();
}

/* 暂停 */
static void cd_btn_pause_cb(lv_event_t* e) {
    if (g_cd_state != CD_RUNNING)
        return;
    g_cd_state = CD_PAUSED;
    cd_update_ui();
}

/* 重置 */
static void cd_btn_reset_cb(lv_event_t* e) {
    g_cd_state = CD_IDLE;
    g_cd_remaining = g_cd_total;
    cd_update_ui();
}

/* 创建右栏: 大字剩余时间 + 分/秒 roller + 三个按钮 */
static lv_obj_t* alarm_col_right_create(lv_obj_t* parent) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(col, false);

    /* 大字剩余时间 */
    g_cd_label = lv_label_create(col);
    lv_font_t* font_big = get_font(FONT_TYPE_CN, 40);
    if (font_big != NULL)
        lv_obj_set_style_text_font(g_cd_label, font_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_cd_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(g_cd_label, "05:00");
    lv_obj_align(g_cd_label, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 分:秒 roller 横条: 靠左下 */
    lv_obj_t* bar = lv_obj_create(col);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 142, 57);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_layout(bar, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(bar, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(bar, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    char opts[256];
    build_roller_options(opts, 59);
    g_cd_roller_m = lv_roller_create(bar);
    lv_obj_remove_style_all(g_cd_roller_m);
    lv_obj_add_style(g_cd_roller_m, &style_alarm_roller, LV_STATE_DEFAULT);
    lv_obj_add_style(g_cd_roller_m, &style_alarm_roller_sel, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_font_t* font_r = get_font(FONT_TYPE_CN, 14); /* 必须先设字体 */
    if (font_r != NULL)
        lv_obj_set_style_text_font(g_cd_roller_m, font_r, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_roller_set_options(g_cd_roller_m, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_cd_roller_m, 3);
    lv_roller_set_selected(g_cd_roller_m, 5, LV_ANIM_OFF); /* 默认 05 */
    lv_obj_set_width(g_cd_roller_m, 64);
    lv_obj_set_style_text_align(g_cd_roller_m, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_cd_roller_m, cd_roller_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* colon = lv_label_create(bar);
    lv_font_t* font_c = get_font(FONT_TYPE_CN, 14);
    if (font_c != NULL)
        lv_obj_set_style_text_font(colon, font_c, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(colon, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_pad_left(colon, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(colon, 2, LV_STATE_DEFAULT);

    g_cd_roller_s = lv_roller_create(bar);
    lv_obj_remove_style_all(g_cd_roller_s);
    lv_obj_add_style(g_cd_roller_s, &style_alarm_roller, LV_STATE_DEFAULT);
    lv_obj_add_style(g_cd_roller_s, &style_alarm_roller_sel, LV_PART_SELECTED | LV_STATE_DEFAULT);
    if (font_r != NULL)
        lv_obj_set_style_text_font(g_cd_roller_s, font_r, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_roller_set_options(g_cd_roller_s, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_cd_roller_s, 3);
    lv_roller_set_selected(g_cd_roller_s, 0, LV_ANIM_OFF); /* 默认 00 */
    lv_obj_set_width(g_cd_roller_s, 64);
    lv_obj_set_style_text_align(g_cd_roller_s, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_cd_roller_s, cd_roller_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 三个小按钮: 靠右下 */
    lv_obj_t* btn_row = lv_obj_create(col);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, 226, 24);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_layout(btn_row, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(btn_row, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_main_place(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(btn_row, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    lv_font_t* font_btn = get_font(FONT_TYPE_CN, 14);

    g_cd_btn_start = lv_btn_create(btn_row);
    lv_obj_remove_style_all(g_cd_btn_start);
    lv_obj_add_style(g_cd_btn_start, &style_alarm_small_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(g_cd_btn_start, &style_alarm_small_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(g_cd_btn_start, &style_alarm_small_btn_disabled, LV_STATE_DISABLED);
    lv_obj_set_size(g_cd_btn_start, 70, 24);
    lv_obj_set_scrollable(g_cd_btn_start, false);
    g_cd_btn_start_label = lv_label_create(g_cd_btn_start);
    if (font_btn != NULL)
        lv_obj_set_style_text_font(g_cd_btn_start_label, font_btn, LV_STATE_DEFAULT);
    lv_label_set_text(g_cd_btn_start_label, "开始");
    lv_obj_center(g_cd_btn_start_label);
    lv_obj_add_event_cb(g_cd_btn_start, cd_btn_start_cb, LV_EVENT_CLICKED, NULL);

    g_cd_btn_pause = lv_btn_create(btn_row);
    lv_obj_remove_style_all(g_cd_btn_pause);
    lv_obj_add_style(g_cd_btn_pause, &style_alarm_small_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(g_cd_btn_pause, &style_alarm_small_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(g_cd_btn_pause, &style_alarm_small_btn_disabled, LV_STATE_DISABLED);
    lv_obj_set_size(g_cd_btn_pause, 70, 24);
    lv_obj_set_scrollable(g_cd_btn_pause, false);
    g_cd_btn_pause_label = lv_label_create(g_cd_btn_pause);
    if (font_btn != NULL)
        lv_obj_set_style_text_font(g_cd_btn_pause_label, font_btn, LV_STATE_DEFAULT);
    lv_label_set_text(g_cd_btn_pause_label, "暂停");
    lv_obj_center(g_cd_btn_pause_label);
    lv_obj_add_event_cb(g_cd_btn_pause, cd_btn_pause_cb, LV_EVENT_CLICKED, NULL);

    g_cd_btn_reset = lv_btn_create(btn_row);
    lv_obj_remove_style_all(g_cd_btn_reset);
    lv_obj_add_style(g_cd_btn_reset, &style_alarm_small_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(g_cd_btn_reset, &style_alarm_small_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(g_cd_btn_reset, &style_alarm_small_btn_disabled, LV_STATE_DISABLED);
    lv_obj_set_size(g_cd_btn_reset, 70, 24);
    lv_obj_set_scrollable(g_cd_btn_reset, false);
    lv_obj_t* reset_label = lv_label_create(g_cd_btn_reset);
    if (font_btn != NULL)
        lv_obj_set_style_text_font(reset_label, font_btn, LV_STATE_DEFAULT);
    lv_label_set_text(reset_label, "重置");
    lv_obj_center(reset_label);
    lv_obj_add_event_cb(g_cd_btn_reset, cd_btn_reset_cb, LV_EVENT_CLICKED, NULL);

    /* 初始: IDLE, 暂停/重置禁用 */
    g_cd_total = 5 * 60;
    g_cd_remaining = g_cd_total;
    lv_obj_set_state(g_cd_btn_pause, LV_STATE_DISABLED, true);
    lv_obj_set_state(g_cd_btn_reset, LV_STATE_DISABLED, true);
    lv_obj_set_state(g_cd_btn_start, LV_STATE_DISABLED, false);

    return col;
}

/* 每秒: 顶栏 WiFi 轮询 + 闹钟到点比对 + 倒计时 tick */
static void alarm_check_timer_cb(lv_timer_t* t) {
    /* 顶栏 WiFi 状态 (1s 粒度够) */
    update_top_wifi_ui();

    /* ---- 闹钟到点比对 (edge 触发, 防每秒重复响) ---- */
    if (g_alarm_switch != NULL && lv_obj_has_state(g_alarm_switch, LV_STATE_CHECKED)) {
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        int set_h = (int)lv_roller_get_selected(g_alarm_roller_h);
        int set_m = (int)lv_roller_get_selected(g_alarm_roller_m);
        if (tm_now.tm_hour == set_h && tm_now.tm_min == set_m) {
            if (!g_alarm_fired) {
                g_alarm_fired = true;
                alarm_play_sound(GET_MUSIC_PATH("audio_warn.wav"));
            }
        } else {
            g_alarm_fired = false; /* 时间错开, 允许下个周期再响 */
        }
    }

    /* ---- 倒计时 tick ---- */
    if (g_cd_state == CD_RUNNING) {
        g_cd_remaining--;
        if (g_cd_remaining <= 0) {
            alarm_play_sound(GET_MUSIC_PATH("audio_finish.wav"));
            g_cd_state = CD_IDLE;
            g_cd_remaining = g_cd_total;
        }
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", (int)(g_cd_remaining / 60), (int)(g_cd_remaining % 60));
        lv_label_set_text(g_cd_label, buf);
        if (g_cd_state != CD_RUNNING)
            cd_update_ui(); /* 结束时复位按钮/roller */
    }
}

/* 闹钟管理组件: 卡片内左右分栏, 左=闹钟设置, 右=倒计时 */
lv_obj_t* alarm_manage_create(lv_obj_t* parent) {
    alarm_manage_style_init();

    /* 根容器: flex row 两栏 + 1px 竖分隔线 */
    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_layout(root, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(root, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(root, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    lv_obj_t* col_left = alarm_col_left_create(root);
    lv_obj_set_flex_grow(col_left, 1);

    lv_obj_t* divider = lv_obj_create(root);
    lv_obj_remove_style_all(divider);
    lv_obj_add_style(divider, &style_alarm_divider, LV_STATE_DEFAULT);
    lv_obj_set_size(divider, 1, LV_PCT(88)); /* 竖线略矮于栏高, 两端留白更柔和 */

    lv_obj_t* col_right = alarm_col_right_create(root);
    lv_obj_set_flex_grow(col_right, 1);

    /* 每秒刷新: 顶栏 WiFi + 闹钟到点 + 倒计时 */
    lv_timer_create(alarm_check_timer_cb, 1000, NULL);
    return root;
}

/* ==================== 蓝牙 / 亮度 / 音量管理组件 (demo4 移植) ==================== */

/* 亮度写入: 板上写背光 sysfs (0-255), 模拟器打印 */
static void brt_apply(int pct) {
#ifdef SIMULATOR_LINUX
    printf("[backlight] %d%%\n", pct);
    fflush(stdout);
#else
    char cmd[96];
    snprintf(cmd, sizeof(cmd), "echo %d > /sys/class/backlight/backlight/brightness", pct * 255 / 100);
    system(cmd);
#endif
}

/* 音量写入: 板上 amixer 调 Soft Volume Master (0-255), 模拟器打印 */
static void vol_apply(int pct) {
#ifdef SIMULATOR_LINUX
    printf("[volume] %d%%\n", pct);
    fflush(stdout);
#else
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "amixer -c 0 sset 'Soft Volume Master' %d%% >/dev/null 2>&1", pct);
    system(cmd);
#endif
}

/* 板上启动时读当前音量 (amixer 输出里的 [xx%]), 模拟器/失败时用 80% */
static int vol_read_init(void) {
#ifndef SIMULATOR_LINUX
    FILE* f = popen("amixer -c 0 sget 'Soft Volume Master' 2>/dev/null", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            const char* pct = strchr(line, '[');
            if (pct != NULL && pct[1] >= '0' && pct[1] <= '9') {
                int v = atoi(pct + 1);
                pclose(f);
                if (v >= 0 && v <= 100)
                    return v;
                return 80;
            }
        }
        pclose(f);
    }
#endif
    return 80;
}

/* 板上启动时读当前背光亮度, 模拟器/失败时用 60% */
static int brt_read_init(void) {
#ifndef SIMULATOR_LINUX
    FILE* f = fopen("/sys/class/backlight/backlight/brightness", "r");
    if (f) {
        int raw = -1;
        if (fscanf(f, "%d", &raw) == 1 && raw >= 0 && raw <= 255) {
            fclose(f);
            return raw * 100 / 255;
        }
        fclose(f);
    }
#endif
    return 60;
}

/* ---------- 蓝牙栏 (纯摆设: 开关只切 UI, 不接真实蓝牙) ---------- */
static lv_obj_t* g_bt_status_label = NULL;

/* 开关点击: CHECKED 态取反, 开关文案 + 状态大字同步 */
static void bt_switch_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    if (lv_obj_has_state(btn, LV_STATE_CHECKED))
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
    else
        lv_obj_add_state(btn, LV_STATE_CHECKED);
    lv_obj_t* label = lv_event_get_user_data(e);
    bool on = lv_obj_has_state(btn, LV_STATE_CHECKED);
    lv_label_set_text(label, on ? "打开" : "关闭");
    lv_label_set_text(g_bt_status_label, on ? "连接" : "未连接");
}

/* 创建蓝牙栏: 标题 + 开关 + 状态大字 */
static lv_obj_t* bt_col_create(lv_obj_t* parent) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(col, false);

    /* 标题 */
    lv_obj_t* title = lv_label_create(col);
    lv_font_t* font_t = get_font(FONT_TYPE_CN, 14);
    if (font_t != NULL)
        lv_obj_set_style_text_font(title, font_t, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0x8A94C8), LV_STATE_DEFAULT);
    lv_label_set_text(title, "蓝牙");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    /* 开关胶囊: 中部 */
    lv_obj_t* sw = lv_btn_create(col);
    lv_obj_remove_style_all(sw);
    lv_obj_add_style(sw, &style_alarm_switch, LV_STATE_DEFAULT);
    lv_obj_add_style(sw, &style_alarm_switch_checked, LV_STATE_CHECKED);
    lv_obj_set_size(sw, 64, 24);
    lv_obj_align(sw, LV_ALIGN_CENTER, 0, -2);
    lv_obj_set_scrollable(sw, false);
    lv_obj_t* sw_label = lv_label_create(sw);
    lv_font_t* font_sw = get_font(FONT_TYPE_CN, 14);
    if (font_sw != NULL)
        lv_obj_set_style_text_font(sw_label, font_sw, LV_STATE_DEFAULT);
    lv_label_set_text(sw_label, "关闭");
    lv_obj_center(sw_label);
    lv_obj_add_event_cb(sw, bt_switch_cb, LV_EVENT_CLICKED, sw_label);

    /* 状态大字 */
    g_bt_status_label = lv_label_create(col);
    lv_font_t* font_big = get_font(FONT_TYPE_CN, 30);
    if (font_big != NULL)
        lv_obj_set_style_text_font(g_bt_status_label, font_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_bt_status_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(g_bt_status_label, "未连接");
    lv_obj_align(g_bt_status_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    return col;
}

/* ---------- 亮度/音量 (右栏, 上下两半) ---------- */
static lv_obj_t* g_brt_label = NULL;
static lv_obj_t* g_vol_label = NULL;

/* 亮度 slider: 拖动中大字实时跟随, 松手才写硬件 (避免拖动时频繁写 sysfs) */
static void brt_slider_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", val);
    lv_label_set_text(g_brt_label, buf);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED)
        brt_apply(val);
}

/* 音量 UI 同步: 右栏大字 + 顶栏 "音量:xx%" 联动 (同一状态只刷一遍) */
static void vol_set_ui(int val) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", val);
    lv_label_set_text(g_vol_label, buf);
    if (g_top_vol_label != NULL) {
        snprintf(buf, sizeof(buf), "音量:%d%%", val);
        lv_label_set_text(g_top_vol_label, buf);
    }
}

/* 音量 slider: 同亮度, 松手写 amixer; 拖动中大字 + 顶栏实时跟随 */
static void vol_slider_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    vol_set_ui(val);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED)
        vol_apply(val);
}

/* 单条调节行: 标题(上) + 大字(左) + slider(右, 垂直居中) */
static void brtvol_row_create(lv_obj_t* parent, const char* title, int init_pct,
                              lv_obj_t** label_out, lv_event_cb_t cb) {
    /* 标题 */
    lv_obj_t* title_l = lv_label_create(parent);
    lv_font_t* font_t = get_font(FONT_TYPE_CN, 14);
    if (font_t != NULL)
        lv_obj_set_style_text_font(title_l, font_t, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title_l, lv_color_hex(0x8A94C8), LV_STATE_DEFAULT);
    lv_label_set_text(title_l, title);
    lv_obj_align(title_l, LV_ALIGN_TOP_MID, 0, 0);

    /* 大字百分比 */
    lv_obj_t* label = lv_label_create(parent);
    lv_font_t* font_v = get_font(FONT_TYPE_CN, 30);
    if (font_v != NULL)
        lv_obj_set_style_text_font(label, font_v, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", init_pct);
    lv_label_set_text(label, buf);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 6);
    *label_out = label;

    /* slider: 点轨道跳转 / 拖动连续调 */
    lv_obj_t* slider = lv_slider_create(parent);
    lv_obj_remove_style_all(slider);
    lv_obj_add_style(slider, &style_bt_slider_main, LV_STATE_DEFAULT);
    lv_obj_add_style(slider, &style_bt_slider_ind, LV_PART_INDICATOR);
    lv_obj_add_style(slider, &style_bt_slider_knob, LV_PART_KNOB);
    lv_obj_set_size(slider, 230, 24);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, init_pct, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, 0, 6);
    lv_obj_set_scrollable(slider, false);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_RELEASED, NULL);
}

/* 创建右栏: 上半亮度, 下半音量 */
static lv_obj_t* brtvol_col_create(lv_obj_t* parent) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(col, false);

    /* 上半: 亮度 (初始值读板上当前背光) */
    lv_obj_t* half1 = lv_obj_create(col);
    lv_obj_remove_style_all(half1);
    lv_obj_set_size(half1, LV_PCT(100), LV_PCT(50));
    lv_obj_align(half1, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_scrollable(half1, false);
    brtvol_row_create(half1, "亮度", brt_read_init(), &g_brt_label, brt_slider_cb);

    /* 下半: 音量 (初始读板上当前音量, 顶栏同步联动) */
    lv_obj_t* half2 = lv_obj_create(col);
    lv_obj_remove_style_all(half2);
    lv_obj_set_size(half2, LV_PCT(100), LV_PCT(50));
    lv_obj_align(half2, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_scrollable(half2, false);
    int vol0 = vol_read_init();
    brtvol_row_create(half2, "音量", vol0, &g_vol_label, vol_slider_cb);
    vol_set_ui(vol0); /* 创建后立刻刷一遍, 顶栏占位文本与实际音量对齐 */

    return col;
}

/* 每秒: 顶栏 WiFi 状态轮询 */
static void bt_check_timer_cb(lv_timer_t* t) {
    update_top_wifi_ui();
}

/* 蓝牙/亮度/音量组件: 卡片内左右分栏, 左=蓝牙摆设, 右=亮度/音量 */
lv_obj_t* bt_setting_create(lv_obj_t* parent) {
    alarm_manage_style_init(); /* 复用开关胶囊/分隔线样式 */
    bt_slider_style_init();

    /* 根容器: flex row 两栏 + 1px 竖分隔线 */
    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_layout(root, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(root, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(root, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    lv_obj_t* col_left = bt_col_create(root);
    lv_obj_set_flex_grow(col_left, 1);

    lv_obj_t* divider = lv_obj_create(root);
    lv_obj_remove_style_all(divider);
    lv_obj_add_style(divider, &style_alarm_divider, LV_STATE_DEFAULT);
    lv_obj_set_size(divider, 1, LV_PCT(88)); /* 竖线略矮于栏高, 两端留白更柔和 */

    lv_obj_t* col_right = brtvol_col_create(root);
    lv_obj_set_flex_grow(col_right, 1);

    /* 每秒刷新顶栏 WiFi */
    lv_timer_create(bt_check_timer_cb, 1000, NULL);
    return root;
}

/* ==================== 日志记录 + 系统更新组件 (demo5 移植) ==================== */

#define SYS_FW_VERSION "v1.0.0" /* 固件版本号 (系统更新部分显示) */

/* ---------- 系统信息采集: 模拟器/板上都是 Linux, 统一读 /proc 和 sysfs ---------- */

/* CPU 使用率: 两次 /proc/stat 采样差 (user+nice+system+idle+iowait+irq+softirq) */
static long g_cpu_total_prev = -1, g_cpu_idle_prev = 0;

static int sys_cpu_pct(void) {
    FILE* f = fopen("/proc/stat", "r");
    if (!f)
        return 0;
    long user, nice, system, idle, iowait, irq, softirq;
    if (fscanf(f, "cpu %ld %ld %ld %ld %ld %ld %ld", &user, &nice, &system, &idle,
               &iowait, &irq, &softirq) != 7) {
        fclose(f);
        return 0;
    }
    fclose(f);
    long total = user + nice + system + idle + iowait + irq + softirq;
    if (g_cpu_total_prev < 0) { /* 首次采样无差值 */
        g_cpu_total_prev = total;
        g_cpu_idle_prev = idle;
        return 0;
    }
    long d_total = total - g_cpu_total_prev;
    long d_idle = idle - g_cpu_idle_prev;
    g_cpu_total_prev = total;
    g_cpu_idle_prev = idle;
    if (d_total <= 0)
        return 0;
    return (int)(100 * (d_total - d_idle) / d_total);
}

/* 内存: /proc/meminfo, 输出 使用中 MB / 总量 MB / 使用百分比 */
static void sys_mem_info(int* used_mb, int* total_mb, int* pct) {
    long total_kb = 0, avail_kb = 0;
    FILE* f = fopen("/proc/meminfo", "r");
    if (f) {
        char key[32];
        long val;
        while (fscanf(f, "%31s %ld", key, &val) == 2) {
            if (strcmp(key, "MemTotal:") == 0)
                total_kb = val;
            else if (strcmp(key, "MemAvailable:") == 0)
                avail_kb = val;
            if (total_kb > 0 && avail_kb > 0)
                break;
        }
        fclose(f);
    }
    if (total_kb <= 0) { /* 读失败兜底 */
        total_kb = 512 * 1024;
        avail_kb = 256 * 1024;
    }
    *total_mb = (int)(total_kb / 1024);
    *used_mb = (int)((total_kb - avail_kb) / 1024);
    *pct = (int)(100 * (total_kb - avail_kb) / total_kb);
}

/* 温度: thermal_zone0 毫度 → 0.1°C 整数 (57.6°C -> 576); 没有则回退 45.0 */
static int sys_temp_c10(void) {
    FILE* f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f) {
        long v;
        if (fscanf(f, "%ld", &v) == 1 && v > 0) {
            fclose(f);
            return (int)(v / 100);
        }
        fclose(f);
    }
    return 450;
}

/* 运行时长: /proc/uptime 秒 */
static long sys_uptime_sec(void) {
    FILE* f = fopen("/proc/uptime", "r");
    if (f) {
        double up;
        if (fscanf(f, "%lf", &up) == 1) {
            fclose(f);
            return (long)up;
        }
        fclose(f);
    }
    return 0;
}

/* 运行时长格式化: "2天23时" / "2时35分" / "5分20秒" */
static void sys_uptime_str(long sec, char* buf, int n) {
    if (sec >= 3600 * 24)
        snprintf(buf, n, "%ld天%ld时", sec / 86400, (sec % 86400) / 3600);
    else if (sec >= 3600)
        snprintf(buf, n, "%ld时%ld分", sec / 3600, (sec % 3600) / 60);
    else
        snprintf(buf, n, "%ld分%ld秒", sec / 60, sec % 60);
}

/* 内核版本: /proc/version "Linux version 5.4.61 (...)" → "5.4.61" */
static void sys_kernel_ver(char* buf, int n) {
    buf[0] = '\0';
    FILE* f = fopen("/proc/version", "r");
    if (!f)
        return;
    char t1[32], t2[32], t3[64];
    if (fscanf(f, "%31s %31s %63s", t1, t2, t3) == 3 && strcmp(t1, "Linux") == 0)
        snprintf(buf, n, "%s", t3);
    fclose(f);
}

/* 1 分钟负载: /proc/loadavg */
static float sys_load1(void) {
    FILE* f = fopen("/proc/loadavg", "r");
    if (f) {
        float a, b, c;
        if (fscanf(f, "%f %f %f", &a, &b, &c) == 3) {
            fclose(f);
            return a;
        }
        fclose(f);
    }
    return 0;
}

/* ---------- 日志: 环形缓冲, 底条显示最近 3 条 ---------- */
#define SYS_LOG_NUM 24  /* 环形缓冲容量 */
#define SYS_LOG_LEN 48  /* 每条上限 (含时间戳) */
static char g_log_buf[SYS_LOG_NUM][SYS_LOG_LEN];
static int g_log_next = 0;   /* 下一写入位置 (环形) */
static int g_log_count = 0;  /* 已写入条数 */
static lv_obj_t* g_log_label = NULL;

/* 追加一条日志, 带 "HH:MM:SS " 时间戳, 刷新底条显示 */
static void sys_log_add(const char* fmt, ...) {
    char msg[SYS_LOG_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    snprintf(g_log_buf[g_log_next], SYS_LOG_LEN, "%02d:%02d:%02d %s",
             tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, msg);
    g_log_next = (g_log_next + 1) % SYS_LOG_NUM;
    if (g_log_count < SYS_LOG_NUM)
        g_log_count++;

    /* 刷新显示: 最近 3 条按时间先后 (环形缓冲可能回绕) */
    if (g_log_label == NULL)
        return;
    char text[SYS_LOG_LEN * 4];
    text[0] = '\0';
    int start = (g_log_next - g_log_count + SYS_LOG_NUM) % SYS_LOG_NUM;
    int shown = 0;
    for (int i = 0; i < g_log_count && shown < 3; i++) {
        const char* line = g_log_buf[(start + i) % SYS_LOG_NUM];
        if (shown > 0)
            strncat(text, "\n", sizeof(text) - strlen(text) - 1);
        strncat(text, line, sizeof(text) - strlen(text) - 1);
        shown++;
    }
    lv_label_set_text(g_log_label, text);
}

/* ---------- 运行状态图表: CPU% + 内存% 双系列折线 ---------- */
static lv_obj_t* g_chart = NULL;
static lv_chart_series_t* g_ser_cpu = NULL;
static lv_chart_series_t* g_ser_mem = NULL;

static void sys_chart_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
    g_chart = lv_chart_create(parent);
    lv_obj_remove_style_all(g_chart);
    lv_obj_set_size(g_chart, w, h);
    lv_chart_set_point_count(g_chart, 30); /* 保留最近 30 个点 (30 秒) */
    lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_update_mode(g_chart, LV_CHART_UPDATE_MODE_SHIFT); /* 新点从右进, 旧点左移 */
    /* 网格线: LV_PART_MAIN 的 line; 系列线: LV_PART_ITEMS 的 line */
    lv_obj_set_style_line_width(g_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_color(g_chart, lv_color_hex(0x9DB4FF), LV_PART_MAIN);
    lv_obj_set_style_line_opa(g_chart, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_line_width(g_chart, 2, LV_PART_ITEMS);
    g_ser_cpu = lv_chart_add_series(g_chart, lv_color_hex(0x6B7BFF), LV_CHART_AXIS_PRIMARY_Y);
    g_ser_mem = lv_chart_add_series(g_chart, lv_color_hex(0xA06BFF), LV_CHART_AXIS_PRIMARY_Y);
}

/* ---------- 左栏: 系统信息 / 系统更新 ---------- */
static lv_obj_t* g_cpu_label = NULL;
static lv_obj_t* g_mem_label = NULL;
static lv_obj_t* g_up_label = NULL; /* 运行时长 + 负载 */

static lv_obj_t* sysinfo_col_create(lv_obj_t* parent) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(col, false);

    /* 标题 */
    lv_obj_t* title = lv_label_create(col);
    lv_font_t* font_t = get_font(FONT_TYPE_CN, 14);
    if (font_t != NULL)
        lv_obj_set_style_text_font(title, font_t, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0x8A94C8), LV_STATE_DEFAULT);
    lv_label_set_text(title, "系统信息");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    /* CPU 大字 */
    g_cpu_label = lv_label_create(col);
    lv_font_t* font_big = get_font(FONT_TYPE_CN, 30);
    if (font_big != NULL)
        lv_obj_set_style_text_font(g_cpu_label, font_big, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_cpu_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(g_cpu_label, "CPU 0%");
    lv_obj_align(g_cpu_label, LV_ALIGN_TOP_MID, 0, 20);

    /* 内存 / 温度 */
    g_mem_label = lv_label_create(col);
    lv_font_t* font_m = get_font(FONT_TYPE_CN, 12);
    if (font_m != NULL)
        lv_obj_set_style_text_font(g_mem_label, font_m, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_mem_label, lv_color_hex(0xE6ECFF), LV_STATE_DEFAULT);
    lv_label_set_text(g_mem_label, "内存 --/--MB  温度 --°C");
    lv_obj_align(g_mem_label, LV_ALIGN_TOP_MID, 0, 60);

    /* 内核 + 固件版本 (静态, 系统更新部分) */
    char kv[16];
    sys_kernel_ver(kv, sizeof(kv));
    lv_obj_t* info = lv_label_create(col);
    if (font_m != NULL)
        lv_obj_set_style_text_font(info, font_m, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(info, lv_color_hex(0xE6ECFF), LV_STATE_DEFAULT);
    lv_label_set_text_fmt(info, "内核 %s  固件 " SYS_FW_VERSION, kv);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 78);

    /* 运行时长 + 负载 */
    g_up_label = lv_label_create(col);
    if (font_m != NULL)
        lv_obj_set_style_text_font(g_up_label, font_m, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_up_label, lv_color_hex(0xE6ECFF), LV_STATE_DEFAULT);
    lv_label_set_text(g_up_label, "运行 --  负载 --");
    lv_obj_align(g_up_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    return col;
}

/* ---------- 右栏: 运行状态图表 + 日志记录 ---------- */
static lv_obj_t* status_col_create(lv_obj_t* parent) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(col, false);

    /* 标题 */
    lv_obj_t* title = lv_label_create(col);
    lv_font_t* font_t = get_font(FONT_TYPE_CN, 14);
    if (font_t != NULL)
        lv_obj_set_style_text_font(title, font_t, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0x8A94C8), LV_STATE_DEFAULT);
    lv_label_set_text(title, "运行状态");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    /* 折线图表: 全宽 × 50, 标题下方 */
    sys_chart_create(col, LV_PCT(100), 50);
    lv_obj_align(g_chart, LV_ALIGN_TOP_MID, 0, 20);

    /* 日志底条: 半透明深色圆角条, 显示最近 3 条事件 */
    lv_obj_t* log_box = lv_obj_create(col);
    lv_obj_remove_style_all(log_box);
    lv_obj_add_style(log_box, &style_sys_log_bg, LV_STATE_DEFAULT);
    lv_obj_set_size(log_box, LV_PCT(100), 44);
    lv_obj_align(log_box, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_scrollable(log_box, false);

    g_log_label = lv_label_create(log_box);
    lv_font_t* font_l = get_font(FONT_TYPE_CN, 12);
    if (font_l != NULL)
        lv_obj_set_style_text_font(g_log_label, font_l, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_log_label, lv_color_hex(0xE6ECFF), LV_STATE_DEFAULT);
    lv_label_set_text(g_log_label, "等待事件…");
    lv_obj_align(g_log_label, LV_ALIGN_LEFT_MID, 10, 0);

    return col;
}

/* ---------- 每秒刷新: 采样 + 图表 + 数值 + WiFi/温度日志 ---------- */
static bool g_hot_logged = false; /* 温度告警日志去重 (回落后可再触发) */

static void sys_timer_cb(lv_timer_t* t) {
    /* 顶栏 WiFi 轮询 */
    update_top_wifi_ui();

    /* 采样 */
    int cpu = sys_cpu_pct();
    int used_mb, total_mb, mem_pct;
    sys_mem_info(&used_mb, &total_mb, &mem_pct);
    int temp = sys_temp_c10();
    char up[24];
    sys_uptime_str(sys_uptime_sec(), up, sizeof(up));

    /* 图表: 每点 1 秒, 滚动 30 点 */
    if (g_chart != NULL) {
        lv_chart_set_next_value(g_chart, g_ser_cpu, cpu);
        lv_chart_set_next_value(g_chart, g_ser_mem, mem_pct);
        lv_chart_refresh(g_chart);
    }

    /* 数值 */
    if (g_cpu_label != NULL)
        lv_label_set_text_fmt(g_cpu_label, "CPU %d%%", cpu);
    if (g_mem_label != NULL)
        lv_label_set_text_fmt(g_mem_label, "内存 %d/%dMB  温度 %d.%d°C",
                              used_mb, total_mb, temp / 10, temp % 10);
    if (g_up_label != NULL)
        lv_label_set_text_fmt(g_up_label, "运行 %s  负载 %.2f", up, sys_load1());

    /* 温度告警日志 (65°C 阈值, 回落后再触发) */
    if (temp >= 650 && !g_hot_logged) {
        g_hot_logged = true;
        sys_log_add("温度过高 %d.%d°C", temp / 10, temp % 10);
    } else if (temp < 650) {
        g_hot_logged = false;
    }
}

/* 日志记录 + 系统更新组件: 卡片内左右分栏, 左=系统信息/版本, 右=图表+日志 */
lv_obj_t* sys_update_create(lv_obj_t* parent) {
    alarm_manage_style_init(); /* 复用分隔线样式 */
    sys_style_init();

    /* 根容器: flex row 两栏 + 1px 竖分隔线 */
    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_layout(root, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(root, LV_FLEX_FLOW_ROW, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(root, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    lv_obj_t* col_left = sysinfo_col_create(root);
    lv_obj_set_flex_grow(col_left, 1);

    lv_obj_t* divider = lv_obj_create(root);
    lv_obj_remove_style_all(divider);
    lv_obj_add_style(divider, &style_alarm_divider, LV_STATE_DEFAULT);
    lv_obj_set_size(divider, 1, LV_PCT(88)); /* 竖线略矮于栏高, 两端留白更柔和 */

    lv_obj_t* col_right = status_col_create(root);
    lv_obj_set_flex_grow(col_right, 1);

    /* 启动日志 + 每秒采样刷新 */
    sys_log_add("系统启动");
    lv_timer_create(sys_timer_cb, 1000, NULL);
    return root;
}

/* ==================== 组件切换: 左侧行点击 → 右侧卡片换内容 ==================== */
typedef enum {
    COMP_WIFI = 0,  /* 网络连接 (demo2 wifi连接条) */
    COMP_NETINFO,   /* 网络设置 (demo1 气泡网络信息) */
    COMP_ALARM,     /* 闹钟管理 */
    COMP_BT,        /* 蓝牙设置 */
    COMP_SYS,       /* 日志记录 / 系统更新 (默认) */
    COMP_MAX
} comp_id_t;

/* 4 个功能组件在卡片内预创建, 切换 = 互斥 hide/show (保留各组件内部状态, 无重建延迟) */
static lv_obj_t* g_comps[COMP_MAX] = {NULL};
static comp_id_t g_cur_comp = COMP_SYS;

static void switch_component(comp_id_t id) {
    if (id == g_cur_comp)
        return;
    /* 离开 WiFi 页时收起屏幕键盘 (键盘建在 screen 上, 不属于任何组件) */
    if (g_cur_comp == COMP_WIFI && g_wifi_kb != NULL)
        lv_obj_add_flag(g_wifi_kb, LV_OBJ_FLAG_HIDDEN);
    if (g_comps[id] != NULL)
        lv_obj_clear_flag(g_comps[id], LV_OBJ_FLAG_HIDDEN);
    if (g_comps[g_cur_comp] != NULL)
        lv_obj_add_flag(g_comps[g_cur_comp], LV_OBJ_FLAG_HIDDEN);
    g_cur_comp = id;
}

/* 行点击回调: user_data = 目标组件 id */
static void row_switch_cb(lv_event_t* e) {
    switch_component((comp_id_t)(intptr_t)lv_event_get_user_data(e));
}

/* 注册中间矩形组件: 状态卡片 + 预创建全部 4 个功能组件 (默认只显示系统信息) */
static lv_obj_t* lv_mid_screen_componnet_create(lv_obj_t* parent) {
    /* 状态卡片: 靠屏幕右半区 (卡片 60% 宽, 从右侧向内收, 不与左侧设置列表重叠) */
    lv_obj_t* sp_panel = status_panel_create(parent, LV_PCT(60), LV_PCT(60));
    lv_obj_align(sp_panel, LV_ALIGN_RIGHT_MID, -20, 0);

    /* 全部预创建: 各自内部状态 (输入内容/倒计时/滑杆值) 在切换间保留,
       每个组件自带的定时器 (连接轮询/闹钟比对/采样) 在后台持续运行 */
    g_comps[COMP_WIFI] = wifi_component_create(sp_panel);
    g_comps[COMP_NETINFO] = net_info_component_create(sp_panel);
    g_comps[COMP_ALARM] = alarm_manage_create(sp_panel);
    g_comps[COMP_BT] = bt_setting_create(sp_panel);
    g_comps[COMP_SYS] = sys_update_create(sp_panel);

    /* 默认显示系统信息, 其余隐藏 */
    g_cur_comp = COMP_SYS;
    for (int i = 0; i < COMP_MAX; i++) {
        if (g_comps[i] != NULL && i != g_cur_comp)
            lv_obj_add_flag(g_comps[i], LV_OBJ_FLAG_HIDDEN);
    }

    return sp_panel;
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
    settings_row_t row_bt = row_add_clickable(panel, IMAGE_PATH "iconfont_bule.png", "蓝牙设置", "显示与亮度");
    settings_row_t row_disp = row_add_clickable(panel, IMAGE_PATH "wangl.png", "网络连接 ", "wifi连接");
    settings_row_t row_update = row_add_clickable(panel, IMAGE_PATH "iconfont_genx.png", "网络设置", "蓝牙设置");
    settings_row_t row_alarm = row_add_clickable(panel, IMAGE_PATH "iconfont_laoz.png", "闹钟管理", "闹钟管理");
    settings_row_t row_log = row_add_clickable(panel, IMAGE_PATH "iconfont_riz.png", "日志记录", "系统更新");

    settings_row_set_selected(&row_log, true); /* 初始选中"日志记录/系统更新"行 (默认组件也是它) */

    /* 行点击 → 右侧卡片切换 (互斥选中由 row_click_cb 负责, 这里是内容切换;
       第 2、3 行都指向 WiFi 组件) */
    lv_obj_add_event_cb(row_bt.row, row_switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)COMP_BT);
    lv_obj_add_event_cb(row_disp.row, row_switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)COMP_WIFI);
    lv_obj_add_event_cb(row_update.row, row_switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)COMP_NETINFO);
    lv_obj_add_event_cb(row_alarm.row, row_switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)COMP_ALARM);
    lv_obj_add_event_cb(row_log.row, row_switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)COMP_SYS);

    lv_obj_t* sp_panel = lv_mid_screen_componnet_create(lv_screen_active());
    lv_obj_align_to(sp_panel, panel, LV_ALIGN_OUT_RIGHT_MID, 66, 0);

    lv_obj_t* btn = bottom_bar_btn_create(bottom_img, "返回"); /* 深蓝玻璃, 与底部栏同色系 */

    /* 字体设在按钮对象上, 经继承链传给内部文案 label */
    lv_font_t* font_btn = get_font(FONT_TYPE_CN, 30);
    if (font_btn != NULL) {
        lv_obj_set_style_text_font(btn, font_btn, LV_STATE_DEFAULT);
    }
}
