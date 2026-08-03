#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_tick.h"
#include "lvgl.h"
#include "page_conf.h"

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

    while (1) {
        lv_timer_handler();
        usleep(5000);
    }
    return 0;
}
