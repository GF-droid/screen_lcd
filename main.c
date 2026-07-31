#include "lvgl/lvgl.h"
#include "lvgl/drivers/sdl/lv_sdl_window.h"
#include "lvgl/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/drivers/sdl/lv_sdl_keyboard.h"

int main(void)
{
    /* 1. 初始化 LVGL */
    lv_init();

    /* 2. 创建 SDL2 窗口作为显示设备 (800x480 分辨率) */
    lv_display_t *disp = lv_sdl_window_create(800, 480);
    lv_sdl_window_set_title(disp, "LVGL v9.6 Demo");

    /* 3. 创建鼠标和键盘输入设备 */
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    /* 4. 创建一个简单的界面 */
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 200, 80);
    lv_obj_center(btn);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Hello LVGL!");
    lv_obj_center(label);

    /* 5. 主循环 */
    while (1) {
        /* 驱动 LVGL 定时器 (处理动画、事件等) */
        uint32_t time_till_next = lv_timer_handler();
        /* 延迟以避免占满 CPU */
        lv_delay_ms(time_till_next);
    }

    return 0;
}
