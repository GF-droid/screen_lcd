#include <stdio.h>
#include "lvgl.h"
#include "page_conf.h"
#include "res_conf.h"


void set_init(void) {
    lv_obj_t* back_image = lv_image_create(lv_screen_active());
    lv_image_set_src(back_image, IMAGE_PATH "back.png");
    lv_obj_set_size(back_image,lv_pct(100), lv_pct(100));
    lv_obj_center(back_image);
}
