#ifndef _RES_CONF_H_
#define _RES_CONF_H_

/*
 * 资源路径配置 (编译期宏, 按平台区分)
 *
 * 使用规则:
 *   1. 在 UI 代码中拼接字符串字面量使用, 例如:
 *        lv_image_set_src(img, IMAGE_PATH "back.png");
 *   2. x86 (SIMULATOR_LINUX): 路径相对程序工作目录, 运行时需 cd 到 apps/demo1/
 *      (LV_FS_POSIX_PATH 为空, "A:" 前缀由 FS 驱动解析)
 *   3. T113: 绝对路径, 资源部署在 /data/res/ (板端 /usr 为 squashfs 只读!)
 *      (LV_FS_POSIX_PATH="/", 无前缀路径走默认驱动字母 'A')
 */

#ifdef SIMULATOR_LINUX
    #define FONT_PATH   "A:./res/font/"
    #define IMAGE_PATH  "A:./res/image/"
#else
    #define FONT_PATH   "A:/data/res/font/"
    #define IMAGE_PATH  "/data/res/image/"
#endif

#endif
