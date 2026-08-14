#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
demo9 云端中继服务器 — T113 智能屏远程控制面板 (aiohttp 单文件)

拓扑:  [T113 板 demo9] --ws--> 本服务器 <--ws-- [浏览器]
板子只做出站连接(Windows 热点 NAT 后无需端口映射), 服务器做中继:
  BS(0x09)    板->服->所有浏览器    板端状态(系统参数/音量/亮度/闹钟/CPU)
  CTRL(0x0B)  浏览器->服->板        音量/亮度/闹钟设置
消息协议(二进制帧, 首字节=类型, 与 apps/demo9 与前端 app.js 保持一致):
  0x01 HELLO     板/浏览器  -> 服    {"v":1,"role":"board"|"client","name":..}
  0x04 HEARTBEAT 双向                {"t":"hb","ts":ms}  服务器原样回显测 RTT
  0x06 STATUS    服 -> 浏览器        {"t":"st","board":"online"|"offline",..}
  0x07 ERROR     双向                {"t":"err","code":N,"msg":..}
  0x09 BS        板 -> 服 -> client  {"t":"bs",..系统参数/音量/亮度/闹钟/CPU..}
  0x0B CTRL      浏览器 -> 服 -> 板   {"t":"ctrl","vol":50}/"bright"/"alarm_h"+"alarm_m"+"alarm_on"

鉴权: /ws/board?token= 与 /ws/client?token= 必须匹配 --token (默认 t113demo)。
用法:  python3 server.py [--host 0.0.0.0] [--port 9000] [--token t113demo]
"""

from __future__ import annotations   # 兼容 Python 3.7-3.9 (str | None 注解)

import argparse
import asyncio
import json
import time

from aiohttp import web

# 消息类型 (与板端/前端保持一致)
T_HELLO = 0x01
T_HB = 0x04
T_STATUS = 0x06
T_ERR = 0x07
T_BS = 0x09
T_CTRL = 0x0B


class Relay:
    """进程内中继状态: 板连接 + 浏览器连接 + 板状态缓存"""

    def __init__(self, token):
        self.token = token
        self.boards = {}      # name -> {"ws": WebSocketResponse, "last_bs": bytes|None}
        self.clients = set()  # set[WebSocketResponse]

    def board_count(self):
        return len(self.boards)

    def client_count(self):
        return len(self.clients)


relay = None


def ok(payload: dict) -> bytes:
    return bytes([T_STATUS]) + json.dumps(payload, ensure_ascii=False).encode()


def err_pkt(code: int, msg: str) -> bytes:
    return bytes([T_ERR]) + json.dumps({"t": "err", "code": code, "msg": msg},
                                       ensure_ascii=False).encode()


def check_token(request) -> str | None:
    """校验 token, 通过返回设备名(缺省用远端地址), 失败返回 None"""
    got = request.query.get("token", "")
    if got != relay.token:
        return None
    return request.query.get("name") or ("board@" + request.remote)


async def ws_board(request):
    name = check_token(request)
    if name is None:
        ws = web.WebSocketResponse()
        await ws.prepare(request)
        await ws.send_bytes(err_pkt(1001, "bad token"))
        await ws.close(code=1008)
        return ws

    ws = web.WebSocketResponse(heartbeat=60)
    await ws.prepare(request)

    # 同名牌子的旧连接先踢掉 (重连/多实例场景)
    old = relay.boards.get(name)
    if old is not None:
        try:
            await old["ws"].close(code=4001)
        except Exception:
            pass
    entry = {"ws": ws, "last_bs": None}
    relay.boards[name] = entry
    print(f"[board] {name} 上线, 当前板数={relay.board_count()}")

    try:
        async for msg in ws:
            if msg.type != web.WSMsgType.BINARY:
                continue
            data = msg.data
            if not data:
                continue
            t = data[0]
            payload = data[1:]
            if t == T_HELLO:
                pass  # 注册已由 URL name 完成; 若 HELLO 带 name 可覆盖
            elif t == T_BS:
                entry["last_bs"] = payload   # 缓存: 新浏览器秒得板状态
                for c in list(relay.clients):
                    try:
                        await c.send_bytes(bytes([T_BS]) + payload)
                    except Exception:
                        relay.clients.discard(c)
            elif t == T_HB:
                # 板心跳: 原样回显给板 (测 RTT), 并顺手广播在线状态
                await ws.send_bytes(bytes([T_HB]) + payload)
                await broadcast_status()
            # 其他类型(CTRL 等)来自板属于异常, 忽略
    except Exception as e:
        print(f"[board] {name} 异常: {e}")
    finally:
        relay.boards.pop(name, None)
        print(f"[board] {name} 离线, 当前板数={relay.board_count()}")
        await broadcast_status()
    return ws


async def ws_client(request):
    if check_token(request) is None:
        ws = web.WebSocketResponse()
        await ws.prepare(request)
        await ws.send_bytes(err_pkt(1001, "bad token"))
        await ws.close(code=1008)
        return ws

    ws = web.WebSocketResponse(heartbeat=60)
    await ws.prepare(request)
    relay.clients.add(ws)
    print(f"[client] {request.remote} 上线, 当前浏览器数={relay.client_count()}")

    # 新浏览器秒得板状态 + 当前在线状态
    for b in relay.boards.values():
        if b["last_bs"] is not None:
            await ws.send_bytes(bytes([T_BS]) + b["last_bs"])
    await ws.send_bytes(ok(status_payload()))

    try:
        async for msg in ws:
            if msg.type != web.WSMsgType.BINARY:
                continue
            data = msg.data
            if not data:
                continue
            t = data[0]
            payload = data[1:]
            if t == T_HB:
                await ws.send_bytes(bytes([T_HB]) + payload)  # 回显测 RTT
            elif t == T_CTRL:
                if relay.boards:
                    b = next(iter(relay.boards.values()))
                    try:
                        await b["ws"].send_bytes(bytes([T_CTRL]) + payload)
                    except Exception:
                        pass
                else:
                    await ws.send_bytes(err_pkt(1002, "board offline"))
            elif t == T_HELLO:
                pass
    except Exception as e:
        print(f"[client] {request.remote} 异常: {e}")
    finally:
        relay.clients.discard(ws)
        print(f"[client] {request.remote} 离线, 当前浏览器数={relay.client_count()}")
    return ws


def status_payload() -> dict:
    return {"t": "st", "board": "online" if relay.board_count() > 0 else "offline",
            "clients": relay.client_count()}


async def broadcast_status():
    pkt = ok(status_payload())
    for c in list(relay.clients):
        try:
            await c.send_bytes(pkt)
        except Exception:
            relay.clients.discard(c)


async def index(request):
    return web.FileResponse("static/index.html")


async def healthz(request):
    return web.json_response({"status": "ok",
                              "boards": relay.board_count(),
                              "clients": relay.client_count(),
                              "ts": time.time()})


def main():
    global relay
    ap = argparse.ArgumentParser(description="T113 智能屏远程控制中继服务器")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=9000)
    ap.add_argument("--token", default="t113demo", help="连接口令(板/浏览器 URL 参数)")
    args = ap.parse_args()

    relay = Relay(args.token)

    app = web.Application()
    app.router.add_get("/", index)
    app.router.add_get("/healthz", healthz)
    app.router.add_get("/ws/board", ws_board)
    app.router.add_get("/ws/client", ws_client)
    app.router.add_static("/static/", "static")

    async def status_worker():
        while True:
            await asyncio.sleep(2)
            try:
                await broadcast_status()
            except Exception:
                pass

    async def status_loop(app_):
        task = asyncio.create_task(status_worker())
        try:
            yield
        finally:
            task.cancel()

    app.cleanup_ctx.append(status_loop)

    print(f"T113 远程控制面板中继: http://{args.host}:{args.port}  (token={args.token})")
    web.run_app(app, host=args.host, port=args.port, print=None)


if __name__ == "__main__":
    main()
