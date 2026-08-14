#include "board_status.h"
#include "net/proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
/* linux/wireless.h 自带 linux/if.h; 勿再包含 net/if.h (ifreq 重定义冲突) */
#include <linux/wireless.h>

static void read_meminfo(board_stat_t *s) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { s->mem_total_kb = s->mem_avail_kb = -1; return; }
    char key[64];
    long val;
    while (fscanf(f, "%63s %ld kB\n", key, &val) == 2) {
        if (!strcmp(key, "MemTotal:")) s->mem_total_kb = (int)val;
        else if (!strcmp(key, "MemAvailable:")) s->mem_avail_kb = (int)val;
    }
    fclose(f);
}

static void read_wireless(board_stat_t *s) {
    FILE *f = fopen("/proc/net/wireless", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strchr(line, '|')) continue;   /* 两行表头都含 | */
        char iface[32];
        int status, link_qual, link_lvl, noise;
        /* 数据行: " wlan0: 0000   54.  -43.  -91 ..." */
        if (sscanf(line, " %31s %d %d. %d. %d", iface, &status,
                   &link_qual, &link_lvl, &noise) == 5) {
            s->rssi = link_lvl - 256;   /* 内核以 0-255 记, dBm = v-256 */
            break;
        }
    }
    fclose(f);
}

static void read_uptime(board_stat_t *s) {
    FILE *f = fopen("/proc/uptime", "r");
    if (f) {
        double up;
        if (fscanf(f, "%lf", &up) == 1) s->uptime_s = (long)up;
        fclose(f);
    }
}

/* CPU 使用率: 读 /proc/stat 第一行两次, 用 (total-idle) 差值占比 */
static uint64_t g_last_total, g_last_idle;

static void read_cpu(board_stat_t *s) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) { s->cpu_pct = -1; return; }
    char tag[16];
    unsigned long long user, nice, sys, idle, iowait, irq, softirq, steal;
    int n = fscanf(f, "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
                   tag, &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
    fclose(f);
    if (n < 9 || strcmp(tag, "cpu") != 0) { s->cpu_pct = -1; return; }
    uint64_t total = user + nice + sys + idle + iowait + irq + softirq + steal;
    if (g_last_total) {
        uint64_t dt = total - g_last_total;
        uint64_t di = (idle + iowait) - g_last_idle;
        s->cpu_pct = (dt > 0) ? (int)((dt - di) * 100 / dt) : 0;
        if (s->cpu_pct < 0) s->cpu_pct = 0;
        if (s->cpu_pct > 100) s->cpu_pct = 100;
    } else {
        s->cpu_pct = 0;   /* 首次采样无差值 */
    }
    g_last_total = total;
    g_last_idle = idle + iowait;
}

static void read_ip_and_ssid(board_stat_t *s) {
    /* 优先找 wlan0/eth0 之类非回环 IPv4 */
    struct ifaddrs *ifas = NULL;
    if (getifaddrs(&ifas) == 0) {
        for (struct ifaddrs *p = ifas; p; p = p->ifa_next) {
            if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
            if (!(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK))
                continue;
            if (!strcmp(p->ifa_name, "lo")) continue;
            struct sockaddr_in *sin = (void *)p->ifa_addr;
            inet_ntop(AF_INET, &sin->sin_addr, s->ip, sizeof(s->ip));
            /* SSID: 用 wlan0 的 ioctl; 其他接口跳过 */
            if (strncmp(p->ifa_name, "wlan", 4) == 0) {
                int fd = socket(AF_INET, SOCK_DGRAM, 0);
                if (fd >= 0) {
                    struct iwreq wr;
                    memset(&wr, 0, sizeof(wr));
                    strncpy(wr.ifr_name, p->ifa_name, IFNAMSIZ - 1);
                    wr.u.essid.pointer = s->ssid;
                    wr.u.essid.length = 63;
                    if (ioctl(fd, SIOCGIWESSID, &wr) == 0 && wr.u.essid.length > 0)
                        s->ssid[wr.u.essid.length > 63 ? 63 : wr.u.essid.length] = '\0';
                    close(fd);
                }
            }
            break;   /* 第一个非回环 IPv4 即可 */
        }
        freeifaddrs(ifas);
    }
}

int board_status_gather(board_stat_t *s) {
    memset(s, 0, sizeof(*s));
    s->mem_total_kb = s->mem_avail_kb = -1;
    s->cpu_pct = -1;
    read_meminfo(s);
    read_wireless(s);
    read_uptime(s);
    read_ip_and_ssid(s);
    read_cpu(s);
    return 0;
}

int board_status_send(demo9_ctx_t *ctx) {
    board_stat_t s;
    board_status_gather(&s);
    /* 控制状态从 ctx->ctrl 取 (ws 线程与 alarm 线程都可能写, int 原子读) */
    s.vol = ctx->ctrl.vol;
    s.bright = ctx->ctrl.bright;
    s.alarm_h = ctx->ctrl.alarm_h;
    s.alarm_m = ctx->ctrl.alarm_m;
    s.alarm_on = ctx->ctrl.alarm_on;
    s.alarm_fired = ctx->ctrl.alarm_fired;
    char buf[512];
    int n = snprintf(buf, sizeof(buf),
                     "{\"t\":\"bs\",\"ssid\":\"%s\",\"ip\":\"%s\","
                     "\"mem_avail_kb\":%d,\"mem_total_kb\":%d,"
                     "\"rssi\":%d,\"uptime\":%ld,\"cpu\":%d,"
                     "\"vol\":%d,\"bright\":%d,"
                     "\"alarm_h\":%d,\"alarm_m\":%d,\"alarm_on\":%d,\"alarm_fired\":%d}",
                     s.ssid, s.ip, s.mem_avail_kb, s.mem_total_kb,
                     s.rssi, s.uptime_s, s.cpu_pct,
                     s.vol, s.bright,
                     s.alarm_h, s.alarm_m, s.alarm_on, s.alarm_fired);
    if (n <= 0 || n >= (int)sizeof(buf)) return -1;
    uint8_t frame[520];
    int flen = proto_build(T_BS, (uint8_t *)buf, n, frame);
    pthread_mutex_lock(&ctx->send_lock);
    int r = ws_send_frame(&ctx->ws, 0x2, frame, flen);
    pthread_mutex_unlock(&ctx->send_lock);
    return r;
}
