#ifndef _NET_PROTO_H_
#define _NET_PROTO_H_

#include <stdint.h>
#include <stdarg.h>

/* 应用层消息打包: 二进制帧首字节=类型, 其余为 payload (JSON 或裸数据) */

/* 构建一帧: out 需能容纳 1+len */
int proto_build(uint8_t type, const uint8_t *payload, int len, uint8_t *out);

/* 发送 JSON 消息 (printf 风格拼 JSON), 返回 0 成功 */
int proto_send_json(void *ws_conn, uint8_t type, const char *fmt, ...);

/* ---------- 极简 JSON 取值 (固定格式, 不做通用解析) ---------- */
/* 提取字符串字段值到 buf (最多 cap-1 字节); 返回 0 找到 */
int json_get_str(const uint8_t *json, int len, const char *key,
                 char *buf, int cap);
/* 提取整数字段; 返回 0 找到 */
int json_get_int(const uint8_t *json, int len, const char *key, int *out);

#endif  // _NET_PROTO_H_
