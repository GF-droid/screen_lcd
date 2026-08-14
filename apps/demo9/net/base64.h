#ifndef _NET_BASE64_H_
#define _NET_BASE64_H_

#include <stdint.h>
#include <stddef.h>

/* base64 编码 (标准表, 无换行), 返回写入 out 的字节数 */
int b64_encode(const uint8_t *in, int in_len, char *out);

#endif  // _NET_BASE64_H_
