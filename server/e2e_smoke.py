#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""demo9 精简版端到端冒烟测试: 模拟浏览器 ws 客户端
用法:  python3 e2e_smoke.py [ws://127.0.0.1:9000] [--token t113demo]"""
import asyncio, json, sys, time
import websockets

TOKEN = "t113demo"
URL = "ws://127.0.0.1:9000"
for i, a in enumerate(sys.argv[1:]):
    if a == "--token" and i + 2 < len(sys.argv):
        TOKEN = sys.argv[i + 2]
    elif a.startswith("ws://"):
        URL = a

T = {"HELLO": 0x01, "HB": 0x04, "STATUS": 0x06, "ERR": 0x07, "BS": 0x09, "CTRL": 0x0B}
BY = {v: k for k, v in T.items()}

async def main():
    uri = f"{URL}/ws/client?token={TOKEN}"
    async with websockets.connect(uri, subprotocols=None) as ws:
        await ws.send(bytes([T["HELLO"]]) + json.dumps(
            {"v": 1, "role": "client", "name": "e2e"}).encode())

        last_bs = {}
        got_status = asyncio.Event()
        got_hb = asyncio.Event()
        timeout = time.time() + 15

        def ok(label):
            print(f"  ✓ {label}")

        def fail(label, extra=""):
            print(f"  ✗ {label} {extra}")
            sys.exit(1)

        async def recv_until():
            nonlocal last_bs
            while True:
                frame = await asyncio.wait_for(ws.recv(), timeout=5)
                d = frame if isinstance(frame, (bytes, bytearray)) else bytes(frame)
                t, payload = d[0], d[1:]
                s = payload.decode() if payload else ""
                if t == T["STATUS"]:
                    st = json.loads(s)
                    if st["board"] == "online":
                        got_status.set()
                elif t == T["BS"]:
                    last_bs = json.loads(s)
                elif t == T["HB"]:
                    got_hb.set()
                elif t == T["ERR"]:
                    e = json.loads(s)
                    print(f"  ERR: {e}")

        task = asyncio.create_task(recv_until())

        # 1) 板在线
        if not await asyncio.wait_for(got_status.wait(), timeout=8):
            fail("板在线状态", "(服务器侧没有板连接)")
        ok("收到 STATUS, 板在线")

        # 2) BS 字段齐全
        while time.time() < timeout and not (
                last_bs.get("cpu") is not None and "vol" in last_bs and
                "alarm_fired" in last_bs):
            await asyncio.sleep(0.1)
        need = ["ssid", "ip", "mem_avail_kb", "mem_total_kb", "rssi", "uptime",
                "cpu", "vol", "bright", "alarm_h", "alarm_m", "alarm_on",
                "alarm_fired"]
        missing = [k for k in need if k not in last_bs]
        if missing:
            fail("BS 字段齐全", f"缺少 {missing}, 实际={last_bs}")
        ok(f"BS 字段齐全 cpu={last_bs['cpu']}% mem={last_bs['mem_avail_kb']}KB")

        # 3) CTRL 音量 → BS 回显
        await ws.send(bytes([T["CTRL"]]) + json.dumps({"t": "ctrl", "vol": 30}).encode())
        while time.time() < timeout and last_bs.get("vol") != 30:
            await asyncio.sleep(0.1)
        if last_bs.get("vol") != 30:
            fail("音量设置回显", f"vol={last_bs.get('vol')}")
        ok("音量 CTRL 生效, BS 回显 vol=30")

        # 4) CTRL 亮度 → BS 回显
        await ws.send(bytes([T["CTRL"]]) + json.dumps({"t": "ctrl", "bright": 15}).encode())
        while time.time() < timeout and last_bs.get("bright") != 15:
            await asyncio.sleep(0.1)
        if last_bs.get("bright") != 15:
            fail("亮度设置回显", f"bright={last_bs.get('bright')}")
        ok("亮度 CTRL 生效, BS 回显 bright=15")

        # 5) CTRL 闹钟 → BS 回显
        await ws.send(bytes([T["CTRL"]]) + json.dumps(
            {"t": "ctrl", "alarm_h": 7, "alarm_m": 30, "alarm_on": 1}).encode())
        while time.time() < timeout and not (
                last_bs.get("alarm_h") == 7 and last_bs.get("alarm_m") == 30
                and last_bs.get("alarm_on") == 1):
            await asyncio.sleep(0.1)
        if last_bs.get("alarm_h") != 7 or last_bs.get("alarm_m") != 30 or \
                last_bs.get("alarm_on") != 1:
            fail("闹钟设置回显", f"{last_bs.get('alarm_h')}:{last_bs.get('alarm_m')} on={last_bs.get('alarm_on')}")
        ok("闹钟 CTRL 生效, BS 回显 07:30 开")

        # 6) 心跳回显 RTT
        await ws.send(bytes([T["HB"]]) + json.dumps({"t": "hb", "ts": int(time.time() * 1000)}).encode())
        try:
            await asyncio.wait_for(got_hb.wait(), timeout=5)
            ok("心跳回显")
        except asyncio.TimeoutError:
            fail("心跳回显")
        task.cancel()
        print("\n全部通过 ✓")

asyncio.run(main())
