#ifndef _RES_CONF_H_
#define _RES_CONF_H_

#ifdef SIMULATOR_LINUX
    #define FONT_PATH "A:./res/font/"
    #define IMAGE_PATH "A:./res/image/"
    #define MUSIC_PATH "/data/res/music/"
#else
    /* 板端 /usr 是 squashfs 只读, 资源必须放 /data (UDISK 分区) */
    #define FONT_PATH "A:/data/res/font/"
    #define IMAGE_PATH "/data/res/image/"
    #define MUSIC_PATH "/data/res/music/"
#endif

#endif