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

#include "widgets/widget_sysinfo.h"
#include "widgets/widget_alarm.h"
#include "widgets/widget_common.h"
#include "setting.h"

/* ==================== 日志记录 + 系统更新组件 (demo5 移植) ==================== */

static lv_style_t style_sys_log_bg; /* 日志底条: 半透明深色圆角条 */
static bool sys_style_inited = false;

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


void sys_style_init(void) {
    if (sys_style_inited)
        return;
    sys_style_inited = true;

    lv_style_init(&style_sys_log_bg);
    lv_style_set_bg_opa(&style_sys_log_bg, LV_OPA_60);
    lv_style_set_bg_color(&style_sys_log_bg, lv_color_hex(0x1A2150));
    lv_style_set_radius(&style_sys_log_bg, 8);
    lv_style_set_border_width(&style_sys_log_bg, 1);
    lv_style_set_border_color(&style_sys_log_bg, lv_color_hex(0x9DB4FF));
    lv_style_set_border_opa(&style_sys_log_bg, LV_OPA_20);
    lv_style_set_pad_all(&style_sys_log_bg, 0);
}

