#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include "page_conf.h"
#include "res_conf.h"

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

// 注册中间矩形组件: 日志记录 + 系统更新卡片 (左半系统信息, 右半运行状态图表 + 日志)
static lv_obj_t* lv_mid_screen_componnet_create(lv_obj_t* parent) {
    /* 状态卡片: 靠屏幕右半区 (卡片 60% 宽, 从右侧向内收, 不与左侧设置列表重叠) */
    lv_obj_t* sp_panel = status_panel_create(parent, LV_PCT(60), LV_PCT(60));
    lv_obj_align(sp_panel, LV_ALIGN_RIGHT_MID, -20, 0);

    sys_update_create(sp_panel); /* 日志/系统更新组件填充卡片 */

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

/* ==================== 日志记录 + 系统更新组件 ==================== */

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
    /* 顶栏 WiFi 轮询: 状态变化时记日志 */
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

    settings_row_set_selected(&row_log, true); /* 初始选中"日志记录/系统更新"行 */

    lv_obj_t* sp_panel = lv_mid_screen_componnet_create(lv_screen_active());
    lv_obj_align_to(sp_panel, panel, LV_ALIGN_OUT_RIGHT_MID, 66, 0);

    lv_obj_t* btn = bottom_bar_btn_create(bottom_img, "返回"); /* 深蓝玻璃, 与底部栏同色系 */

    /* 字体设在按钮对象上, 经继承链传给内部文案 label */
    lv_font_t* font_btn = get_font(FONT_TYPE_CN, 30);
    if (font_btn != NULL) {
        lv_obj_set_style_text_font(btn, font_btn, LV_STATE_DEFAULT);
    }
}
