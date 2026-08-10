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

#include "widgets/widget_netinfo.h"
#include "widgets/widget_common.h"

/* ==================== 网络信息气泡组件 (demo1 移植: 气泡展示网络相关信息) ==================== */

/* 8 个气泡的文案 label, 由定时器周期刷新为真实数据 (WiFi 状态/IP/掩码) */
static lv_obj_t* g_net_chip_labels[8] = {0};

/* 取活动网卡 (非 lo 且 UP) 的 IPv4 地址+掩码; 找不到返回 false */
static bool get_net_addr(char* ip, size_t ip_sz, char* mask, size_t mask_sz) {
    struct ifaddrs* ifa0 = NULL;
    if (getifaddrs(&ifa0) != 0)
        return false;
    bool found = false;
    for (struct ifaddrs* ifa = ifa0; ifa != NULL && !found; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (strcmp(ifa->ifa_name, "lo") == 0)
            continue;
        if ((ifa->ifa_flags & IFF_UP) == 0)
            continue;
        struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
        struct sockaddr_in* sm = (struct sockaddr_in*)ifa->ifa_netmask;
        inet_ntop(AF_INET, &sa->sin_addr, ip, ip_sz);
        if (sm != NULL && sm->sin_addr.s_addr != 0)
            inet_ntop(AF_INET, &sm->sin_addr, mask, mask_sz);
        else
            snprintf(mask, mask_sz, "255.255.255.0"); /* 掩码缺失兜底 */
        found = true;
    }
    freeifaddrs(ifa0);
    return found;
}

/* WiFi 连接状态 → 气泡文案 (g_conn_status 由 wpa 事件线程写, 主循环轮询读) */
static const char* wifi_status_text(WPA_WIFI_CONNECT_STATUS_E s) {
    switch (s) {
        case WPA_WIFI_CONNECT:    return "WIFI已连接";
        case WPA_WIFI_DISCONNECT: return "WIFI未连接";
        case WPA_WIFI_WRONG_KEY:  return "WIFI密码错误";
        case WPA_WIFI_SCANNING:   return "WIFI扫描中";
        default:                  return "WIFI未启动"; /* INACTIVE */
    }
}

/* 刷新一次真实数据 (创建时 + 定时器周期各调一次) */
static void net_info_refresh(void) {
    if (g_net_chip_labels[0] != NULL)
        lv_label_set_text(g_net_chip_labels[0], wifi_status_text(g_conn_status));

    char ip[INET_ADDRSTRLEN] = {0}, mask[INET_ADDRSTRLEN] = {0};
    char buf[64];
    if (get_net_addr(ip, sizeof(ip), mask, sizeof(mask))) {
        if (g_net_chip_labels[2] != NULL) {
            snprintf(buf, sizeof(buf), "IP:%s", ip);
            lv_label_set_text(g_net_chip_labels[2], buf);
        }
        if (g_net_chip_labels[4] != NULL) {
            snprintf(buf, sizeof(buf), "掩码:%s", mask);
            lv_label_set_text(g_net_chip_labels[4], buf);
        }
    }
}

/* 周期刷新 (1s): WiFi 状态/IP/掩码 跟随真实状态 */
static void net_info_refresh_cb(lv_timer_t* t) {
    (void)t;
    net_info_refresh();
}

/* 气泡 + 图标: 图标缩到 16px 宽放气泡左侧 (与 demo1 lv_image_vewer_create_with_size 一致).
   label_out 非空时返回文案 label 指针 (child[1]), 供周期刷新真实数据用 */
static lv_obj_t* net_chip_with_icon(lv_obj_t* parent, const char* icon_path, lv_coord_t w, lv_coord_t h, const char* text, lv_obj_t** label_out) {
    lv_obj_t* chip = chip_bubble_create(parent, w, h, text);
    if (label_out != NULL)
        *label_out = lv_obj_get_child(chip, 1); /* child[0] 图标槽, child[1] 文案 label */
    lv_obj_t* img = lv_image_create(chip);
    lv_image_set_src(img, icon_path);
    lv_coord_t src_w = lv_image_get_src_width(img); /* 原图宽度 */
    if (src_w > 0) {
        lv_image_set_scale(img, 256 * 16 / src_w); /* 按宽度缩到 16px, 高度等比 */
    }
    lv_obj_set_ignore_layout(img, true); /* 不参与 flex 布局, 由手动 align 定位 */
    lv_obj_align_to(img, chip, LV_ALIGN_LEFT_MID, -5, 0);

    return chip;
}

/* 网络信息组件: 标题 + 深色底板 (flex 换行) + 8 个信息气泡 (demo1 布局) */
lv_obj_t* net_info_component_create(lv_obj_t* parent) {
    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), 172); /* 高 172: 覆盖气泡第二行底 228 (root 顶=卡片 y56) */
    lv_obj_set_scrollable(root, false);

    /* 标题: 30px 白字 "状态卡片", 挂 root 上 (随组件一起隐藏, 不会与别的组件标题叠加) */
    lv_obj_t* label = lv_label_create(root);
    lv_font_t* font = get_font(FONT_TYPE_CN, 30);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, LV_STATE_DEFAULT);
    }
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_label_set_text(label, "状态卡片");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

    /* 深色内容底板: 手动对齐.
       LVGL v9.6: 子对象 pos(0,0) 和 align 的基准是父的内容区起点 (父原点 + pad 24)!
       root 在内容区 (481,80). 与 demo1 几何一致: temp 高 94 (LV_PCT(80) 取整),
       BOTTOM_MID +15 → 绝对底 215, 上缘 121 → 相对 root: y = 121-80 = 41.
       x 必须为 0: root 已在内容区 x481, 再偏移会把一行 5 个气泡 (800px) 顶出
       内容区右边界 x1287 (481+24+806=1287 是根底板的右缘), 最后气泡被截断 */
    lv_obj_t* temp = lv_obj_create(root);
    lv_obj_remove_style_all(temp);
    lv_obj_set_size(temp, LV_PCT(100), 94);
    lv_obj_align(temp, LV_ALIGN_TOP_LEFT, 0, 41);
    lv_obj_set_clickable(temp, false);
    lv_obj_set_scrollable(temp, false);
    /* flex 自动换行: 从左往右排, 一行放不下自动换下一行, 间距 25px (与 demo1 一致) */
    lv_obj_set_style_layout(temp, LV_LAYOUT_FLEX, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(temp, LV_FLEX_FLOW_ROW_WRAP, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(temp, 25, LV_STATE_DEFAULT); /* 列间距 */
    lv_obj_set_style_pad_row(temp, 25, LV_STATE_DEFAULT);    /* 行间距 */

    lv_font_t* fontback = get_font(FONT_TYPE_CN, 10);
    if (fontback != NULL) {
        lv_obj_set_style_text_font(root, fontback, LV_STATE_DEFAULT);
    }

    /* 气泡由 flex 自动排列成行: 140px 宽, 一行 5 个, 8 个换第二行 (demo1 同款).
       带真实数据的三个气泡导出 label 指针: 0=WiFi状态, 2=IP, 4=掩码 */
    net_chip_with_icon(temp, IMAGE_PATH "iconfont_wifi.png", 140, 30, "WIFI办公室", &g_net_chip_labels[0]);
    net_chip_with_icon(temp, IMAGE_PATH "lany.png", 140, 30, "蓝牙办公室", NULL);
    net_chip_with_icon(temp, IMAGE_PATH "iconfont_ip.png", 140, 30, "IP地址:127.0.0.1", &g_net_chip_labels[2]);
    net_chip_with_icon(temp, IMAGE_PATH "ceshi.png", 140, 30, "网络测试", NULL);
    net_chip_with_icon(temp, IMAGE_PATH "xianshima.png", 140, 30, "子掩饰码", &g_net_chip_labels[4]);
    net_chip_with_icon(temp, IMAGE_PATH "iconfont_guan.png", 140, 30, "待定", NULL); /* demo1 无此图, 图标区空白, 保持一致 */
    net_chip_with_icon(temp, IMAGE_PATH "iconfont_bule.png", 140, 30, "待定", NULL);
    net_chip_with_icon(temp, IMAGE_PATH "iconfont_guan.png", 140, 30, "待定", NULL);

    /* 创建即刷一次真实值 (定时器首跳前不显示旧占位文本), 之后 1s 周期跟随 */
    net_info_refresh();
    lv_timer_create(net_info_refresh_cb, 1000, NULL);

    return root;
}
