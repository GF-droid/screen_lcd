# screen_lcd — 桌面超宽智能屏幕（T113-S3）

> 从零独立完成的一台桌面智能屏：自主硬件设计（原理图 → PCB → 焊接 134 个元器件）+
> 嵌入式软件开发（Linux + LVGL）+
> 物联网远程控制（自研 WebSocket 协议 + 云中继服务器）。
>
> 双平台开发体系：x86 Linux SDL2 模拟器快速迭代 + T113-S3 真机交叉编译部署。

---

## 一、项目概述

一款 **1424×280 超宽横屏桌面智能屏**，作为桌面信息终端，展示时间、天气、系统状态等
信息，并支持通过网页远程控制音量、亮度、闹钟。

| 项目 | 内容 |
|---|---|
| 主控 | 全志 T113-S3 双核处理器（Tina Linux） |
| 屏幕 | 1424×280 横屏 LCD + Goodix gt9xx 电容触摸 |
| 显示框架 | LVGL v9.6（硬件加速：全志 G2D /dev/g2d） |
| 开发语言 | C（嵌入式）/ Python（构建与测试脚本） |
| 网络 | WiFi（wpa_supplicant）+ WebSocket（自研客户端） |
| 硬件设计 | 嘉立创EDA 原理图 + 4 层 PCB，134 个元件手工焊接 |

## 二、功能特性

- **桌面信息显示**：LVGL 图形界面，时间、天气、系统状态等信息卡片
- **WiFi 联网**：自研 wpa_manager 组件管理连接与断线检测
- **远程控制**：网页端实时控制面板 —— 音量（amixer）、亮度（backlight 驱动）、
  闹钟（定时响铃 + 本地持久化）
- **系统状态可视化**：板端每 2s 上报 CPU / 内存 / WiFi 信号 / 音量 / 亮度 / 闹钟，
  网页以数值 + 实时折线图呈现
- **双平台验证**：同一套代码在 x86 模拟器与 T113 真机运行，自动化测试脚本支撑

## 三、系统架构

```
┌─────────────────────────────────────────────────────────┐
│  应用层  apps/demo1 ~ demo9                              │
│          桌面信息屏 UI（demo8）  ·  远程控制 agent（demo9）│
├─────────────────────────────────────────────────────────┤
│  组件层  component/                                      │
│          osal（线程/队列封装）  wifi（wpa_manager）       │
│          font（字体工具）                                │
├─────────────────────────────────────────────────────────┤
│  平台层  platform/t113（真机） / platform/x86linux（模拟）│
│          hal：audio / brightness / console / system     │
│                / time / uart                            │
│          porting：lv_port_disp / indev / tick + G2D     │
├─────────────────────────────────────────────────────────┤
│  硬件    T113-S3 · 1424×280 LCD · gt9xx 触摸 · WiFi      │
│          · 4 路 DC-DC 供电 · 背光升压 · 音频功放         │
└─────────────────────────────────────────────────────────┘
```

### 远程控制链路（demo9）

```
[浏览器控制面板] ──ws──┐                       ┌──ws── [T113 板 demo9 agent]
 音量/亮度/闹钟滑块      ├─> [VPS 中继服务器] <─┤  出站连接 + 控制执行 + 状态采集
 状态数值/实时曲线       └──────────────────────┘  线程：ws(连接/心跳/上报) + alarm(响铃)
```

- 板子只做**出站连接**（Windows 热点 NAT 后无需端口映射），服务器（aiohttp 单文件）做中继
- 自研二进制帧协议：`[1字节类型][JSON]`，6 种消息
  （HELLO / HEARTBEAT / STATUS / ERROR / BS 状态上报 / CTRL 控制指令）
- 手写 WebSocket 客户端（约千行）：SHA1+GUID 握手校验、帧解析（掩码/分片/长度扩展）、
  发送掩码 —— 零第三方依赖
- 可靠性设计：应用层心跳 5s（服务器回显测 RTT）、15s 无入站帧判假死强连、
  断线指数退避重连（1s → 30s）
- 板端状态采集：/proc/stat 差值算 CPU%、/proc/net/wireless 解析 RSSI、
  meminfo / uptime / SIOCGIWESSID 拿 SSID，每 2s 组装上报
- 控制落地：amixer 'Soft Volume Master'（音量）、/sys/class/backlight（亮度 0-255）、
  aplay 闹钟 + /data/app/alarm.conf 持久化
- 服务器：token 鉴权、同名旧连接踢除（kick-old-by-name）、last_bs 缓存
  （新浏览器秒得板状态）、每 2s 广播在线状态

## 四、关键技术难点与解决

### 1. 显示旋转体系（最核心）

- 内核 framebuffer 是 **720×1424 竖屏**，物理面板是 **1424×280 横屏**（相差 90°）
- 内容在 **flush 层用 G2D 硬件旋转**（G2D_ROT_270）搬入 fb，LVGL 端不做软件预转
- **触摸坐标必须做互逆变换**：触摸控制器按竖屏报告（X:0~280, Y:0~1424），
  LVGL 逻辑屏是横屏 —— `INDEV_SWAP_XY` 转置 + `INDEV_FLIP_X` 水平镜像修正
- 经验沉淀：显示旋转与触摸旋转是一套互逆变换，只做一半（显示）另一半（触摸）就点击失灵；
  方向问题在应用层解决，**不要改内核 fb 分辨率**（会破坏 line_length 与 mmap 布局）

### 2. 性能优化：1fps → 流畅（三个根因叠加）

| 根因 | 修复 |
|---|---|
| `LV_USE_SYSMON 1` 意外开启：FPS 监视标签每 140ms 刷新 → 自拖垮帧率 | 关闭 SYSMON |
| 图片缓存未开启：全屏 PNG 每帧重新解码（760ms/帧） | `LV_CACHE_DEF_SIZE 4MB`（注意 v9 单位是字节，v8 旧宏 `LV_IMG_CACHE_DEF_SIZE` 在 v9 无效） |
| 排查用实验代码残留 | 调试代码标记清理清单，逐项核对 |

### 3. 平台差异坑

- **CMake 变量 ≠ C 宏**：`-DSIMULATOR_LINUX` 必须 `add_definitions` 且在
  add_subdirectory 之前，否则兄弟目录源码看不到，x86 误走 T113 资源路径
- **内存配置要双平台同步**：x86 的 LVGL 内存池 64KB vs T113 的 20MB ——
  全屏 PNG 解码需要 1.6MB+，x86 白屏根因
- **"无报错"先怀疑日志**：`LV_USE_LOG` / `LV_LOG_PRINTF` 双开关
- 板端 `/usr` 是 squashfs 只读，资源必须放 `/data/`；x86 相对路径依赖 cwd
  （用 `/proc/self/exe` 定位项目根并 chdir 解决）

### 4. 板端调试方法论

- 双平台对照：x86 可自动化（xdotool 模拟点击），用它区分"UI 代码问题"和"板端问题"
- adb + fb 抓帧 + numpy 像素级验证（ARGB8888 → reshape 找目标区域，200 采样点匹配率 97%+）
- 进程残留/僵尸清理（`killall -9` + md5sum 核对部署版本，避免"以为部署了"假象）
- 串口 Ctrl+C 失效（ISIG 被清）→ 自编 ttyfix 工具恢复 termios

## 五、项目演进（渐进式开发）

| Demo | 内容 |
|---|---|
| demo1 | LVGL 基础显示 + PNG 运行时解码（lodepng + POSIX FS） |
| demo2 | WiFi 连接（wpa_manager 组件） |
| demo3 ~ demo7 | UI 功能模块渐进开发（组件化重构） |
| demo8 | 桌面信息屏完整 UI（设置页：WiFi/时间/闹钟/亮度/音量） |
| demo9 | 远程控制 agent：自研 WebSocket + 云中继 + 状态上报 |

9 个 demo 从"点亮屏幕"到"公网远程控制"，每一级都独立可运行、可验证。

## 六、目录结构

```
screen_lcd/
├── build.sh                  # -linux / -t113 / -clean
├── CMakeLists.txt            # 双平台构建入口
├── component/                # 组件层：osal / wifi / font
├── platform/
│   ├── t113/                 # 真机：hal + porting(disp/indev/tick/g2d)
│   └── x86linux/             # x86 SDL2 模拟器平台移植
├── apps/
│   ├── demo1 ~ demo8         # UI 应用渐进演进
│   └── demo9/                # 远程控制 agent
│       ├── net/              # 手写 WebSocket（ws_client/sha1/base64/proto）
│       ├── ctrl/             # control(音量/亮度) + alarm(闹钟线程)
│       └── status/           # board_status 2s 状态采集上报
├── server/                   # 云端中继服务器（aiohttp）+ 控制面板前端
├── html/                     # 项目介绍页（设计思路/元器件选型）
└── tools/                    # png2lvgl / ttyfix / ws_board_sim 测试模拟器
```

## 七、构建与部署

```bash
./build.sh -linux     # x86 模拟器（SDL2，开发迭代）
./build.sh -t113      # T113 交叉编译（arm-openwrt-linux-gcc 8.3）

# 真机部署（adb）
adb push build/t113/apps/demo8/main /data/main
adb shell chmod +x /data/main && adb shell -t /data/main

# 远程控制（demo9 + 云端）
python3 server/server.py --port 9000 --token t113demo   # VPS 上运行
# 板端运行 demo9 agent → 浏览器打开控制面板 → 实时控制/查看状态
```

## 八、成果与数据

- 独立完成"画板 → 采购 → 焊接 → 驱动 → UI → 联网 → 远程控制"全流程，形成完整产品
- 134 个元器件（0603 贴片 / SOT-23 / FPC 座等）手工焊接一次点亮
- 解决嵌入式显示最常见的"显示与触摸方向不一致"核心难题，沉淀可复用旋转映射方案
- 远程控制全链路经自动化测试（Python 冒烟脚本）与真机验证稳定运行
- 状态上报 2s 粒度、心跳 5s、断线自动重连（指数退避）—— 链路自愈

> 内部问题排查记录见 `项目复盘.md`（20 个问题的现象/根因/解决方案/经验沉淀）。
