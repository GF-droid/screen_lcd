#ifndef _FONT_CONF_H_
#define _FONT_CONF_H_

#include "res_conf.h"
#include "font_utils.h"

typedef enum
{
    FONT_TYPE_CN = 0,
    FONT_TYPE_CN_LIGHT,
    FONT_TYPE_NUMBER,
}FONT_TYPE;

#define FONT_TYPE_CN_PATH FONT_PATH "QingNiaoHuaGuangJianMeiHei-2.ttf"
#define FONT_TYPE_CN_LIGHT_PATH FONT_PATH "AaLaoJieZhaoPaiTi-2.ttf"
#define FONT_TYPE_NUMBER_PATH FONT_PATH "ZiHunLongTengKaiShu(ShangYongXuShouQuan)-2.ttf"

#define font_init() \
    do { \
        add_font(FONT_TYPE_CN,FONT_TYPE_CN_PATH); \
        add_font(FONT_TYPE_CN_LIGHT,FONT_TYPE_CN_LIGHT_PATH); \
        add_font(FONT_TYPE_NUMBER,FONT_TYPE_NUMBER_PATH); \
    } while (0)  

#endif