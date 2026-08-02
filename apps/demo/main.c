#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_tick.h"

extern void ui_init(void);

int main(void)
{
    // 初始化 LVGL 库
    lv_init();
    lv_port_tick_init();
    lv_port_disp_init();
    lv_port_indev_init();

    ui_init();

    while (1) {
        lv_timer_handler();
        usleep(5000);
    }
    return 0;
}
