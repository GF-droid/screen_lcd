#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
假板模拟器 — 本地验证服务器/前端用 (开发工具, 不上板)
行为对齐真板 demo9: HELLO → 每 0.5s 推一张生成的 JPEG 帧 (带时间戳+移动方块)
→ 收 TOUCH 打印 (含坐标换算验证: 逻辑 x,y → 注入 raw ABS_X=y, ABS_Y=1423-x)
用法: python3 tools/ws_board_sim.py [ws://127.0.0.1:9000/ws/board] [--token t113demo]
"""
import argparse
import asyncio
import io
import json
import random
import sys
import time

from PIL import Image, ImageDraw

T_HELLO = 0x01
T_FRAME = 0x02
T_TOUCH = 0x03
T_HB = 0x04
T_BS = 0x09
T_CMD = 0x08
T_CMD_RES = 0x0A

SCREEN_W, SCREEN_H = 1424, 280


def gen_frame(t_ms: int) -> bytes:
    """生成一张测试帧: 深色底 + 移动方块 + 时间戳 (真板会推实际屏幕 JPEG)"""
    img = Image.new("RGB", (SCREEN_W, SCREEN_H), (17, 20, 46))
    d = ImageDraw.Draw(img)
    # 移动方块 (验证帧率/变化检测)
    x = int((t_ms / 300) % (SCREEN_W - 120))
    d.rounded_rectangle([x, 60, x + 90, 150], radius=18, fill=(60, 224, 200))
    # 玻璃卡片
    d.rounded_rectangle([SCREEN_W - 320, 20, SCREEN_W - 20, 100], radius=16,
                        fill=(17, 20, 46), outline=(157, 180, 255), width=2)
    d.text((SCREEN_W - 300, 45), "SIMULATOR", fill=(157, 180, 255))
    # 时间戳文字 (每秒更新 → 验证变化/静止节奏)
    ts = time.strftime("%H:%M:%S")
    d.text((20, 20), ts, fill=(232, 236, 255))
    d.text((20, 180), f"frame@{(t_ms // 1000) % 60}", fill=(154, 164, 200))
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=75)
    return buf.getvalue()


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("url", nargs="?", default="ws://127.0.0.1:9000/ws/board")
    ap.add_argument("--token", default="t113demo")
    args = ap.parse_args()

    url = args.url + ("&" if "?" in args.url else "?") + f"token={args.token}"

    while True:
        try:
            import websockets
            async with websockets.connect(url, max_size=2**22) as ws:
                print(f"[sim] 已连接 {url}")
                # HELLO
                await ws.send(bytes([T_HELLO]) + json.dumps(
                    {"v": 1, "role": "board", "name": "t113-sim"}).encode())
                t0 = time.monotonic()
                while True:
                    # 每 0.5s 推帧 (心跳帧节律由真板控制, 这里固定节奏)
                    try:
                        msg = await asyncio.wait_for(ws.recv(), timeout=0.5)
                    except asyncio.TimeoutError:
                        msg = None
                    if msg:
                        if isinstance(msg, bytes):
                            t = msg[0]
                            payload = msg[1:]
                            if t == T_TOUCH:
                                j = json.loads(payload.decode())
                                # 逆变换 (与真板注入一致): ABS_X = ly, ABS_Y = 1423 - lx
                                raw_x = j["y"]
                                raw_y = 1423 - j["x"]
                                print(f"[sim] TOUCH {j['t']:<5} 逻辑({j['x']},{j['y']}) "
                                      f"→ 注入 ABS_X={raw_x} ABS_Y={raw_y}")
                            elif t == T_CMD:
                                j = json.loads(payload.decode())
                                out = f"simulated: {j.get('cmd', '')}"
                                res = json.dumps({"t": "cmd_res", "id": j.get("id", 0),
                                                  "code": 0, "cmd": j.get("cmd", ""),
                                                  "out": out}).encode()
                                await ws.send(bytes([T_CMD_RES]) + res)
                            elif t == T_HB:
                                await ws.send(bytes([T_HB]) + payload)  # 回显 RTT
                    # 推帧
                    frame = gen_frame(int(time.monotonic() * 1000))
                    await ws.send(bytes([T_FRAME]) + frame)
                    await asyncio.sleep(0.5)
        except Exception as e:
            print(f"[sim] 连接断开 ({e}), 2s 后重连…")
            await asyncio.sleep(2)


if __name__ == "__main__":
    asyncio.run(main())
