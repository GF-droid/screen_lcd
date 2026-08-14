#ifndef _STATUS_BOARD_STATUS_H_
#define _STATUS_BOARD_STATUS_H_

#include "../demo9.h"

/* 板端状态采集: /proc/meminfo + /proc/net/wireless + getifaddrs + wifi ioctl
 * 结果组装成 T_BS (0x09) 发给服务器, 每 10s 一次 */

typedef struct {
    char ssid[64];
    char ip[64];
    int mem_avail_kb;   /* -1 = 未知 */
    int mem_total_kb;
    int rssi;           /* dBm, 0 = 未知 */
    long uptime_s;
    int cpu_pct;        /* 0-100, -1 = 未知 (两次 /proc/stat 差值) */
    int vol;            /* 当前音量 0-100 (来自 ctrl 状态) */
    int bright;         /* 当前亮度 0-100 */
    int alarm_h, alarm_m, alarm_on, alarm_fired;
} board_stat_t;

/* 采集一次; 返回 0 成功 (个别字段失败置 -1/空) */
int board_status_gather(board_stat_t *s);
/* 组 0x09 帧并发送 (加发送锁); 返回 0 成功 */
int board_status_send(demo9_ctx_t *ctx);

#endif  // _STATUS_BOARD_STATUS_H_
