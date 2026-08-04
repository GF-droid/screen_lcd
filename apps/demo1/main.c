#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_tick.h"
#include "lvgl.h"
#include "page_conf.h"
#include "wpa_manager.h"

#ifdef SIMULATOR_LINUX
#include <limits.h>
#include <string.h>

/*
 * x86 模拟器: 资源路径是相对 cwd 的 (./res/), 从任意目录运行会找不到图片。
 * 启动时根据 /proc/self/exe 定位项目里的 apps/demo1 目录并 chdir 过去,
 * 这样不管从哪里启动程序, 资源都能加载。
 */
static void chdir_to_app_dir(void) {
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0)
        return;
    exe[n] = '\0';

    /* exe 形如 <项目根>/build/linux/apps/demo1/main, 截断到项目根 */
    char* p = strstr(exe, "/build/");
    if (!p)
        return;
    *p = '\0';
    if (chdir(exe) != 0)
        return;
    chdir("apps/demo1"); /* 资源目录 (cwd 下应有 ./res/) */
}
#endif

/* ================= WiFi 移植测试 (临时) ================= */

static const char* connect_status_name(WPA_WIFI_CONNECT_STATUS_E s) {
    switch (s) {
        case WPA_WIFI_INACTIVE:   return "INACTIVE(未启动)";
        case WPA_WIFI_SCANNING:   return "SCANNING(扫描中)";
        case WPA_WIFI_DISCONNECT: return "DISCONNECT(未连接)";
        case WPA_WIFI_CONNECT:    return "CONNECT(连接成功)";
        case WPA_WIFI_WRONG_KEY:  return "WRONG_KEY(密码错误)";
        default:                  return "UNKNOWN";
    }
}

/* 硬件层事件回调: 连接状态一变, 这里就会被调用 */
static void wifi_connect_cb(WPA_WIFI_CONNECT_STATUS_E status) {
    printf("[回调] WiFi 连接状态变化 -> %d (%s)\n", status, connect_status_name(status));
}

/* 测试入口: 演示 open / add_callback / status / connect 四个 API */
static void wifi_test_run(void) {
    printf("\n========== WiFi 功能测试开始 ==========\n");

    wpa_manager_add_callback(NULL, wifi_connect_cb);
    printf("[1] 注册回调完成\n");

    int ret = wpa_manager_open();
    printf("[2] wpa_manager_open 返回 %d (0=线程创建成功)\n", ret);

    for (int i = 0; i < 8; i++) {
        sleep(1);
        if (i == 2) {
            printf("[3] 主动查询一次状态 (STATUS 命令)\n");
            wpa_manager_wifi_status();
        }
        if (i == 4) {
            printf("[4] 尝试连接 WiFi (无硬件时会打印 wpa_supplicant 未连接)\n");
            wpa_ctrl_wifi_info_t info = {.ssid = "TEST_SSID", .psw = "12345678"};
            wpa_manager_wifi_connect(&info);
        }
        printf("    测试进行中... %ds\n", i + 1);
    }

    printf("========== WiFi 测试结束, 退出 ==========\n");
    exit(0); /* 测试完直接退出, 不进入 LVGL 主循环 */
}
/* ================= WiFi 测试结束 ================= */

int main(void) {
#ifdef SIMULATOR_LINUX
    chdir_to_app_dir();
#endif

    // 初始化 LVGL 库
    lv_init();
    lv_port_tick_init();
    lv_port_disp_init();
    lv_port_indev_init();

    font_init();
    set_init();

    /* ================= WiFi 移植功能测试 (临时, 学习用) ================= */
    wifi_test_run();

    while (1) {
        lv_timer_handler();
        usleep(5000);
    }
    return 0;
}
