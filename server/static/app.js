/* T113 远程控制面板前端 — WebSocket 中继客户端
   协议与板端一致: 二进制帧, 首字节=类型 (见 server/server.py 头注释) */
"use strict";

// ---- 消息类型 (与 server.py / 板端 cfg.h 一致) ----
const T = {
  HELLO: 0x01, HB: 0x04, STATUS: 0x06, ERR: 0x07, BS: 0x09, CTRL: 0x0B,
};

const TOKEN = new URLSearchParams(location.search).get("token") || "t113demo";
const $ = (id) => document.getElementById(id);

// ---- DOM ----
const badges = {
  ws: $("badge-ws"), board: $("badge-board"), latency: $("badge-latency"),
};
const banner = $("banner");
const volSlider = $("vol-slider"), volVal = $("vol-val");
const brightSlider = $("bright-slider"), brightVal = $("bright-val");
const alarmH = $("alarm-h"), alarmM = $("alarm-m"), alarmOn = $("alarm-on");
const alarmState = $("alarm-state"), ringing = $("ringing");
const chartCanvas = $("chart"), chartCtx = chartCanvas.getContext("2d");

// ---- 状态 ----
let ws = null;
let boardOnline = false;
let retryDelay = 2000;
let lastRtt = -1;
let alarmUiInit = false;   // 首次 BS 回填闹钟控件, 之后不打扰用户操作
const CHART_POINTS = 60;   // 2s/点 × 60 = 2 分钟窗口
let chartData = [];        // [{cpu, mem}]
const chartStart = Date.now();
let hoverIdx = -1;         // 图表 hover 的采样点下标 (-1 = 无)

// ---- 闹钟控件初始化 (时 0-23 / 分 0-59) ----
(function () {
  for (let h = 0; h < 24; h++) {
    const o = document.createElement("option");
    o.value = h; o.textContent = String(h).padStart(2, "0");
    alarmH.appendChild(o);
  }
  for (let m = 0; m < 60; m++) {
    const o = document.createElement("option");
    o.value = m; o.textContent = String(m).padStart(2, "0");
    alarmM.appendChild(o);
  }
})();

// ---- 发送二进制帧 [type + json] ----
function send(t, obj) {
  if (!ws || ws.readyState !== WebSocket.OPEN) return false;
  const body = new Uint8Array([t, ...new TextEncoder().encode(JSON.stringify(obj))]);
  ws.send(body);
  return true;
}

// ---- 滑块本地预览 + 渐变填充 ----
function bindSlider(slider, valEl, ctrlKey) {
  slider.addEventListener("input", () => {
    valEl.textContent = slider.value + "%";
    slider.style.setProperty("--fill", slider.value + "%");
  });
  slider.addEventListener("change", () => {   // release 时才发, 拖动不发 (与 demo8 语义一致)
    valEl.textContent = slider.value + "%";
    slider.style.setProperty("--fill", slider.value + "%");
    send(T.CTRL, { t: "ctrl", [ctrlKey]: parseInt(slider.value, 10) });
  });
}
bindSlider(volSlider, volVal, "vol");
bindSlider(brightSlider, brightVal, "bright");

// ---- 闹钟: 时/分/开关任一变化都整体发送 ----
function alarmChanged() {
  send(T.CTRL, {
    t: "ctrl",
    alarm_h: parseInt(alarmH.value, 10),
    alarm_m: parseInt(alarmM.value, 10),
    alarm_on: alarmOn.checked ? 1 : 0,
  });
  alarmState.textContent = alarmOn.checked ? "开" : "关";
  alarmState.classList.toggle("on", alarmOn.checked);
}
alarmH.addEventListener("change", alarmChanged);
alarmM.addEventListener("change", alarmChanged);
alarmOn.addEventListener("change", alarmChanged);

// ---- 折线图 (Canvas 自绘, 零依赖: 网格 + 面积渐变 + 发光线 + hover 十字) ----
function fitCanvas() {
  const dpr = window.devicePixelRatio || 1;
  chartCanvas.width = Math.max(400, chartCanvas.clientWidth) * dpr;
  chartCanvas.height = 300 * dpr;
  chartCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
}
function px(p) { return { x: padL + (p / (CHART_POINTS - 1)) * plotW, y: padT + plotH - (Math.min(Math.max(p, 0), 100) / 100) * plotH }; }
let padL, padR, padT, padB, plotW, plotH;

function drawChart() {
  const cw = chartCanvas.clientWidth, ch = 300;
  padL = 36; padR = 12; padT = 16; padB = 24;
  plotW = cw - padL - padR; plotH = ch - padT - padB;
  chartCtx.clearRect(0, 0, cw, ch);

  // 网格 + Y 轴刻度 (0/25/50/75/100)
  chartCtx.strokeStyle = "rgba(157,180,255,0.09)";
  chartCtx.fillStyle = "#9aa4c8";
  chartCtx.font = "11px ui-monospace, monospace";
  chartCtx.textAlign = "right";
  for (let g = 0; g <= 4; g++) {
    const y = padT + plotH - (g / 4) * plotH;
    chartCtx.beginPath();
    chartCtx.moveTo(padL, y);
    chartCtx.lineTo(cw - padR, y);
    chartCtx.stroke();
    chartCtx.fillText((g * 25) + "%", padL - 8, y + 4);
  }

  // X 轴时间刻度 (每 20s)
  chartCtx.textAlign = "center";
  for (let s = 0; s <= 120; s += 20) {
    const x = padL + (s / 120) * plotW;
    const t = new Date(chartStart + s * 1000);
    chartCtx.fillText(`${String(t.getMinutes()).padStart(2, "0")}:${String(t.getSeconds()).padStart(2, "0")}`,
                      x, ch - 8);
  }

  if (chartData.length < 2) {
    chartCtx.fillStyle = "rgba(154,164,200,0.7)";
    chartCtx.textAlign = "center";
    chartCtx.fillText("等待设备数据...", cw / 2, ch / 2);
    return;
  }

  const series = [
    { key: "cpu", color: "#3ce0c8", grad: "rgba(60,224,200,", glow: "rgba(60,224,200,0.35)" },
    { key: "mem", color: "#9db4ff", grad: "rgba(157,180,255,", glow: "rgba(157,180,255,0.35)" },
  ];

  for (const s of series) {
    const pts = chartData.map((d, i) => ({ ...px(d[s.key]), i }));

    // 面积渐变填充
    const grad = chartCtx.createLinearGradient(0, padT, 0, padT + plotH);
    grad.addColorStop(0, s.grad + "0.22)");
    grad.addColorStop(1, s.grad + "0)");
    chartCtx.beginPath();
    chartCtx.moveTo(pts[0].x, padT + plotH);
    pts.forEach(p => chartCtx.lineTo(p.x, p.y));
    chartCtx.lineTo(pts[pts.length - 1].x, padT + plotH);
    chartCtx.closePath();
    chartCtx.fillStyle = grad;
    chartCtx.fill();

    // 发光线
    chartCtx.beginPath();
    pts.forEach((p, i) => i === 0 ? chartCtx.moveTo(p.x, p.y) : chartCtx.lineTo(p.x, p.y));
    chartCtx.strokeStyle = s.color;
    chartCtx.lineWidth = 2.2;
    chartCtx.shadowColor = s.glow;
    chartCtx.shadowBlur = 9;
    chartCtx.stroke();
    chartCtx.shadowBlur = 0;

    // 最新点光点
    const last = pts[pts.length - 1];
    chartCtx.beginPath();
    chartCtx.arc(last.x, last.y, 4.5, 0, Math.PI * 2);
    chartCtx.fillStyle = s.color;
    chartCtx.shadowColor = s.glow;
    chartCtx.shadowBlur = 12;
    chartCtx.fill();
    chartCtx.shadowBlur = 0;
  }

  // hover 十字线 + 数值提示
  if (hoverIdx >= 0 && hoverIdx < chartData.length) {
    const d = chartData[hoverIdx];
    const x = padL + (hoverIdx / (CHART_POINTS - 1)) * plotW;
    chartCtx.strokeStyle = "rgba(232,236,255,0.35)";
    chartCtx.lineWidth = 1;
    chartCtx.setLineDash([4, 4]);
    chartCtx.beginPath();
    chartCtx.moveTo(x, padT);
    chartCtx.lineTo(x, padT + plotH);
    chartCtx.stroke();
    chartCtx.setLineDash([]);
    // 提示框
    const label = `CPU ${d.cpu}% · 内存 ${d.mem}%`;
    chartCtx.font = "12px ui-monospace, monospace";
    const tw = chartCtx.measureText(label).width + 18;
    let bx = x + 10; if (bx + tw > cw - padR) bx = x - 10 - tw;
    chartCtx.fillStyle = "rgba(17,20,46,0.92)";
    chartCtx.strokeStyle = "rgba(60,224,200,0.4)";
    chartCtx.beginPath();
    chartCtx.roundRect(bx, padT - 6, tw, 22, 8);
    chartCtx.fill();
    chartCtx.stroke();
    chartCtx.fillStyle = "#e8ecff";
    chartCtx.textAlign = "left";
    chartCtx.fillText(label, bx + 9, padT + 9);
  }
}

// 图表鼠标跟随
chartCanvas.addEventListener("mousemove", (ev) => {
  const r = chartCanvas.getBoundingClientRect();
  const x = ev.clientX - r.left;
  const i = Math.round((x - padL) / (plotW / (CHART_POINTS - 1)));
  if (i >= 0 && i < chartData.length) hoverIdx = i;
  else hoverIdx = -1;
  drawChart();
});
chartCanvas.addEventListener("mouseleave", () => { hoverIdx = -1; drawChart(); });
window.addEventListener("resize", () => { fitCanvas(); drawChart(); });

// ---- 连接 ----
function connect() {
  const proto = location.protocol === "https:" ? "wss" : "ws";
  const url = `${proto}://${location.host}/ws/client?token=${encodeURIComponent(TOKEN)}`;
  banner.hidden = true;
  badges.ws.className = "badge";
  badges.ws.lastChild.textContent = "连接中…";

  ws = new WebSocket(url);
  ws.binaryType = "arraybuffer";

  ws.onopen = () => {
    badges.ws.classList.add("on");
    badges.ws.lastChild.textContent = "已连接";
    retryDelay = 2000;
    send(T.HELLO, { v: 1, role: "client", name: "web" });
  };

  ws.onmessage = (ev) => {
    if (typeof ev.data !== "object") return;
    const d = new Uint8Array(ev.data);
    if (!d.length) return;
    const t = d[0];
    const s = new TextDecoder().decode(d.subarray(1));
    switch (t) {
      case T.STATUS: {
        const st = JSON.parse(s);
        boardOnline = st.board === "online";
        setBadge(badges.board, boardOnline, boardOnline ? "设备在线" : "设备离线");
        break;
      }
      case T.HB: {
        const hb = JSON.parse(s);
        if (hb.ts) {
          lastRtt = Math.max(0, Date.now() - hb.ts);
          badges.latency.textContent = `RTT ${lastRtt}ms`;
        }
        break;
      }
      case T.ERR: {
        const e = JSON.parse(s);
        console.warn("[err]", e.code, e.msg);
        break;
      }
      case T.BS: onBoardStatus(s);
    }
  };

  ws.onclose = () => {
    badges.ws.classList.add("off");
    badges.ws.lastChild.textContent = "断开";
    setBadge(badges.board, false, "设备离线");
    banner.hidden = false;
    setTimeout(connect, retryDelay);
    retryDelay = Math.min(retryDelay * 2, 15000);
  };
  ws.onerror = () => ws.close();
}

function setBadge(el, on, text) {
  el.classList.remove("on", "off");
  el.classList.add(on ? "on" : "off");
  el.lastChild.textContent = text;
}

// 数值更新 + 脉冲高亮
function setStat(id, text, pulse) {
  const el = $(id);
  if (!el) return;
  el.textContent = text;
  if (pulse) {
    el.classList.remove("flash");
    void el.offsetWidth;   // 重启动画
    el.classList.add("flash");
  }
}

// ---- 板端状态 (BS) ----
function onBoardStatus(s) {
  let b;
  try { b = JSON.parse(s); } catch { return; }
  if (b.t !== "bs") return;

  // 系统参数 (每次更新脉冲一下)
  setStat("st-ip", b.ip || "--", true);
  setStat("st-ssid", b.ssid || "--", true);
  setStat("st-rssi", b.rssi ? `${b.rssi} dBm` : "--", true);
  if (b.mem_total_kb > 0) {
    const used = Math.max(0, Math.round((1 - b.mem_avail_kb / b.mem_total_kb) * 100));
    setStat("st-mem", `${used}% (${(b.mem_avail_kb / 1024).toFixed(0)}MB 可用)`, true);
  } else setStat("st-mem", "--");
  setStat("st-cpu", b.cpu != null ? `${b.cpu}%` : "--", true);
  if (b.uptime_s != null) {
    const h = Math.floor(b.uptime_s / 3600), m = Math.floor((b.uptime_s % 3600) / 60);
    setStat("st-uptime", h > 0 ? `${h}h${m}m` : `${m}m`, true);
  }
  setStat("st-vol", b.vol != null ? `${b.vol}%` : "--", true);
  setStat("st-bright", b.bright != null ? `${b.bright}%` : "--", true);

  // 闹钟: 首次 BS 回填控件 (之后不覆盖用户操作)
  if (!alarmUiInit && b.alarm_h != null && b.alarm_m != null) {
    alarmUiInit = true;
    alarmH.value = b.alarm_h;
    alarmM.value = b.alarm_m;
    alarmOn.checked = !!b.alarm_on;
    alarmState.textContent = b.alarm_on ? "开" : "关";
    alarmState.classList.toggle("on", !!b.alarm_on);
    if (b.vol != null) volSlider.value = b.vol;
    if (b.bright != null) brightSlider.value = b.bright;
    volVal.textContent = volSlider.value + "%";
    brightVal.textContent = brightSlider.value + "%";
    volSlider.style.setProperty("--fill", volSlider.value + "%");
    brightSlider.style.setProperty("--fill", brightSlider.value + "%");
  }
  ringing.hidden = !b.alarm_fired;

  // 折线图: CPU% + 内存使用率%
  const memPct = b.mem_total_kb > 0
    ? Math.max(0, Math.round((1 - b.mem_avail_kb / b.mem_total_kb) * 100)) : 0;
  chartData.push({ cpu: b.cpu != null ? b.cpu : 0, mem: memPct });
  if (chartData.length > CHART_POINTS) chartData.shift();
  drawChart();
}

// 心跳: 每 5s 发 (服务器回显测 RTT)
setInterval(() => {
  if (ws && ws.readyState === WebSocket.OPEN)
    send(T.HB, { t: "hb", ts: Date.now() });
}, 5000);

fitCanvas();
connect();
