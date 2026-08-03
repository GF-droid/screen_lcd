#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "font_utils.h"

typedef struct
{
    int type;
    char font_url[250];
} font_type_t;

typedef struct {
    int type;
    uint16_t size;
    lv_font_t* font;           // Tiny TTF 直接存
#if LV_USE_FREETYPE
    lv_ft_info_t* ft_info;     // FreeType 需要保留（用于销毁时释放）
#endif
} font_obj_t;


static bool is_init = false;
static lv_ll_t font_type_list;
static lv_ll_t font_obj_list;

static font_type_t* find_font_type(int type) {
    font_type_t* font_type = _lv_ll_get_head(&font_type_list);
    while (font_type) {
        if (font_type->type == type) {
            return font_type;
        }
        font_type = _lv_ll_get_next(&font_type_list, font_type);
    }
    return NULL;
}

static lv_font_t* create_font_obj(int type, uint16_t size) {
    font_type_t* font_type = find_font_type(type);
    if (font_type == NULL) {
        return NULL;
    }

#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
    // Tiny TTF 方式
    printf("[font] Loading: %s (size=%d)\n", font_type->font_url, size);
    lv_font_t* font = lv_tiny_ttf_create_file(font_type->font_url, size);
    if (font == NULL) {
        printf("[font] ERROR: Failed to load font!\n");
        return NULL;
    }
    printf("[font] Success! font=%p, line_height=%d\n", (void*)font, font->line_height);
#elif LV_USE_FREETYPE
    // FreeType 方式（保留作为回退）
    lv_ft_info_t* ft_info = malloc(sizeof(lv_ft_info_t));
    ft_info->name = font_type->font_url;
    ft_info->weight = size;
    ft_info->style = FT_FONT_STYLE_NORMAL;
    ft_info->mem = NULL;
    ft_info->mem_size = 0;
    if (!lv_ft_font_init(ft_info)) {
        free(ft_info);
        return NULL;
    }
    // 注意：这里还需要调整 font_obj_t 结构体...
#else
#error "No font engine enabled (LV_USE_TINY_TTF or LV_USE_FREETYPE)"
#endif

    font_obj_t* font_obj = _lv_ll_ins_tail(&font_obj_list);
    font_obj->type = type;
    font_obj->size = size;
    font_obj->font = font;  // ← Tiny TTF 直接存 font
    // font_obj->ft_info = ft_info;  // ← FreeType 需要改为存 ft_info
    return font;
}

void add_font(int type, char* font_url) {
    if (!is_init) {
        is_init = true;
        _lv_ll_init(&font_obj_list, sizeof(font_obj_t));
        _lv_ll_init(&font_type_list, sizeof(font_type_t));
    }
    font_type_t* font_type = _lv_ll_ins_tail(&font_type_list);
    font_type->type = type;
    strcpy(font_type->font_url, font_url);
}
// 获取字体对象
lv_font_t* get_font(int type, uint16_t size) {
    font_obj_t* font_obj = _lv_ll_get_head(&font_obj_list);
    while (font_obj) {
        if (font_obj->type == type && font_obj->size == size) {
            return font_obj->font;  // 直接返回
        }
        font_obj = _lv_ll_get_next(&font_obj_list, font_obj);
    }
    return create_font_obj(type, size);
}
