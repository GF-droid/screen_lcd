#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
screen_lcd 项目介绍页 — Python/Flask 后端

启动:  python3 server.py            (默认 http://127.0.0.1:8000)
       python3 server.py --port 9000
       python3 server.py --no-api    (纯静态模式)

路由:
  GET  /                项目介绍页
  GET  /api/project     项目实时统计(代码行数 / 最近提交 / 屏幕规格等, 由本服务动态生成)
  GET  /static/*        静态资源 (css / js)
  GET  /assets/*        截图资源
"""

import argparse
import datetime
import json
import os
import subprocess
from pathlib import Path

from flask import Flask, jsonify, send_from_directory

BASE_DIR = Path(__file__).resolve().parent
PROJECT_DIR = BASE_DIR.parent          # 项目根目录 screen_lcd/

app = Flask(__name__, static_folder=str(BASE_DIR / "static"), static_url_path="/static")

# 截图资源 (相对 html/assets/)
@app.route("/assets/<path:filename>")
def assets(filename: str):
    return send_from_directory(BASE_DIR / "assets", filename)


# ---------------------------------------------------------------------------
# 项目统计: 由后端在启动时扫描真实代码, 不是写死的假数据
# ---------------------------------------------------------------------------
SKIP_DIRS = {"build", ".git", "html", "lvgl-master", "toolchain"}


def count_lines(path: Path, exts: set) -> dict:
    """递归统计指定扩展名的文件数与代码行数 (跳过符号链接避免循环)"""
    files, lines = 0, 0
    if path.is_file():
        if path.suffix in exts:
            files, lines = 1, sum(1 for _ in path.open(encoding="utf-8", errors="ignore"))
        return files, lines
    if not path.is_dir():
        return files, lines
    for p in sorted(path.iterdir()):
        if p.name in SKIP_DIRS or p.is_symlink():
            continue
        f, l = count_lines(p, exts)
        files += f
        lines += l
    return files, lines


def git_info() -> dict:
    """最近一次 git 提交信息"""
    try:
        r = subprocess.run(
            ["git", "-C", str(PROJECT_DIR), "log", "-1", "--format=%h %s (%ad)", "--date=format:%Y-%m-%d"],
            capture_output=True, text=True, timeout=5,
        )
        parts = r.stdout.strip().split(" ", 1) if r.stdout.strip() else ["", "无提交记录"]
        return {"short_hash": parts[0], "message": parts[1] if len(parts) > 1 else ""}
    except Exception:
        return {"short_hash": "?", "message": "git 不可用"}


def build_project_info() -> dict:
    c_files, c_lines = count_lines(PROJECT_DIR, {".c", ".h"})
    py_files, py_lines = count_lines(BASE_DIR, {".py"})
    commits = 0
    try:
        r = subprocess.run(
            ["git", "-C", str(PROJECT_DIR), "rev-list", "--count", "HEAD"],
            capture_output=True, text=True, timeout=5,
        )
        commits = int(r.stdout.strip() or 0)
    except Exception:
        commits = 0
    return {
        "project": "screen_lcd",
        "description": "T113-S3 超宽智能屏幕 · LVGL 9.6 玻璃拟态 UI",
        "screen": {"width": 1424, "height": 280, "ratio": "5.1:1"},
        "platforms": ["x86 Linux 模拟器", "T113-S3 真机"],
        "ui_framework": "LVGL v9.6",
        "language": {"C": f"{c_lines} 行 / {c_files} 文件", "Python": f"{py_lines} 行 / {py_files} 文件"},
        "git_commits": commits,
        "last_commit": git_info(),
        "generated_at": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
    }


PROJECT_INFO = build_project_info()


@app.route("/")
def index():
    return send_from_directory(BASE_DIR, "index.html")


@app.route("/api/project")
def api_project():
    """项目实时统计 — 每次请求刷新 generated_at, 其余数据启动时扫描一次"""
    PROJECT_INFO["generated_at"] = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    return jsonify(PROJECT_INFO)


@app.route("/healthz")
def healthz():
    return jsonify({"status": "ok", "time": datetime.datetime.now().strftime("%H:%M:%S")})


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="screen_lcd 项目介绍页后端")
    parser.add_argument("--port", type=int, default=8000, help="监听端口 (默认 8000)")
    parser.add_argument("--host", default="127.0.0.1", help="监听地址 (默认 127.0.0.1)")
    parser.add_argument("--no-api", action="store_true", help="禁用 /api 端点(纯静态)")
    args = parser.parse_args()

    if args.no_api:
        # 纯静态模式: 把 /api/project 替换成一个占位响应, 避免前端报错
        @app.route("/api/project")
        def _api_static():
            return jsonify({"note": "后端以 --no-api 模式运行, 仅提供静态页面"})

    print(f" * screen_lcd 介绍页: http://{args.host}:{args.port}")
    app.run(host=args.host, port=args.port, debug=False)
