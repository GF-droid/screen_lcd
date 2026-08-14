#ifndef _NET_SHA1_H_
#define _NET_SHA1_H_

#include <stdint.h>
#include <stddef.h>

/* RFC3174 SHA-1, 零依赖自实现 (WebSocket 握手用) */
void sha1(const uint8_t *data, size_t len, uint8_t out[20]);

#endif  // _NET_SHA1_H_
