/* ==========================================================================
   screen_lcd 介绍页交互
   粒子星空 / 打字机 / 滚动显现 / 鼠标光晕 / 数字滚动 / 截图轮播 / 终端打字
   ========================================================================== */

(function () {
  "use strict";

  /* ---------- 1. 粒子星空 ---------- */
  const canvas = document.getElementById("stars");
  const ctx = canvas.getContext("2d");
  let stars = [], W, H;

  function resize() {
    W = canvas.width = window.innerWidth;
    H = canvas.height = window.innerHeight;
  }
  resize();
  window.addEventListener("resize", resize);

  const COLORS = ["#3ce0c8", "#9db4ff", "#7c6cff", "#e8ecff"];
  function spawnStar(anywhere) {
    return {
      x: Math.random() * W,
      y: anywhere ? Math.random() * H : H + 10,
      r: Math.random() * 1.6 + 0.4,
      vy: Math.random() * 0.45 + 0.08,
      vx: (Math.random() - 0.5) * 0.15,
      c: COLORS[(Math.random() * COLORS.length) | 0],
      tw: Math.random() * Math.PI * 2,
      tws: Math.random() * 0.03 + 0.008,
    };
  }
  for (let i = 0; i < 130; i++) stars.push(spawnStar(true));

  function drawStars(t) {
    ctx.clearRect(0, 0, W, H);
    for (let i = 0; i < stars.length; i++) {
      const s = stars[i];
      s.tw += s.tws;
      s.y -= s.vy; s.x += s.vx;
      if (s.y < -10 || s.x < -10 || s.x > W + 10) { stars[i] = spawnStar(false); continue; }
      const a = 0.35 + 0.55 * (0.5 + 0.5 * Math.sin(s.tw));
      ctx.globalAlpha = a;
      ctx.fillStyle = s.c;
      ctx.shadowColor = s.c;
      ctx.shadowBlur = 8;
      ctx.beginPath();
      ctx.arc(s.x, s.y, s.r, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
    ctx.shadowBlur = 0;
    requestAnimationFrame(drawStars);
  }
  requestAnimationFrame(drawStars);

  /* ---------- 2. 鼠标光晕 + 卡片光照 ---------- */
  const glow = document.getElementById("mouseGlow");
  let glowX = -9999, glowY = -9999, glowRafId = null;
  window.addEventListener("pointermove", (e) => {
    /* pointermove 高频触发(每帧可能多次): 只记录坐标, 零开销;
       实际更新合并到 requestAnimationFrame, 每帧最多一次 style recalc */
    glowX = e.clientX;
    glowY = e.clientY;
    if (glowRafId) return;
    glowRafId = requestAnimationFrame(() => {
      glowRafId = null;
      /* 光晕: left/top 直接定位(负 margin 让中心对准坐标), 与 e.clientX/Y 同一坐标系,
         不经 transform/合成层 — 避免 Chrome 在缩放/高分屏下 fixed+transform 的合成偏移 */
      glow.style.left = glowX + "px";
      glow.style.top = glowY + "px";
      document.querySelectorAll(".card").forEach((c) => {
        const r = c.getBoundingClientRect();
        /* 光斑中心 = 鼠标在卡片内的相对位置(百分比), x/y 都要给, 否则光斑钉在卡片顶部 */
        c.style.setProperty("--mx", ((glowX - r.left) / r.width * 100) + "%");
        c.style.setProperty("--my", ((glowY - r.top) / r.height * 100) + "%");
      });
    });
  });

  /* ---------- 3. 滚动显现 + 导航栏 ---------- */
  const io = new IntersectionObserver((entries) => {
    entries.forEach((en) => {
      if (en.isIntersecting) { en.target.classList.add("in"); io.unobserve(en.target); }
    });
  }, { threshold: 0.12 });
  document.querySelectorAll(".reveal").forEach((el) => io.observe(el));

  const nav = document.getElementById("navbar");
  window.addEventListener("scroll", () => {
    nav.classList.toggle("scrolled", window.scrollY > 30);
  }, { passive: true });

  /* ---------- 4. 数字滚动 ---------- */
  function animateCount(el) {
    const target = parseFloat(el.dataset.count);
    const decimal = parseInt(el.dataset.decimal || "0");
    const dur = 1400, t0 = performance.now();
    function tick(t) {
      const p = Math.min(1, (t - t0) / dur);
      const eased = 1 - Math.pow(1 - p, 3);
      el.textContent = (target * eased).toFixed(decimal);
      if (p < 1) requestAnimationFrame(tick);
      else el.textContent = target.toFixed(decimal);
    }
    requestAnimationFrame(tick);
  }
  const countIO = new IntersectionObserver((entries) => {
    entries.forEach((en) => {
      if (en.isIntersecting) { animateCount(en.target); countIO.unobserve(en.target); }
    });
  }, { threshold: 0.6 });
  document.querySelectorAll("[data-count]").forEach((el) => countIO.observe(el));

  /* ---------- 5. 打字机 ---------- */
  const TEXTS = [
    "一块 1424×280 的超宽条屏,摆在显示器下面,时间、天气、状态排成一行。",
    "从内核驱动到 UI 都是自己写的 — LVGL 9.6 画界面,T113-S3 跑系统。",
    "WiFi 连上网,浏览器里拖个滑块,板子的音量亮度就跟着变。",
  ];
  const typer = document.getElementById("typer");
  let ti = 0, ci = 0, deleting = false;

  function typeTick() {
    const text = TEXTS[ti];
    if (!deleting) {
      ci++;
      if (ci >= text.length) { deleting = true; setTimeout(typeTick, 2200); return; }
    } else {
      ci -= 2;
      if (ci <= 0) { deleting = false; ti = (ti + 1) % TEXTS.length; ci = 0; }
    }
    typer.innerHTML = text.slice(0, Math.max(0, ci)) + '<span class="caret"></span>';
    setTimeout(typeTick, deleting ? 22 : 55);
  }
  setTimeout(typeTick, 400);

  /* ---------- 6. 截图轮播 ---------- */
  const imgs = document.querySelectorAll(".screen-img");
  const thumbs = document.querySelectorAll(".thumb");
  const caption = document.getElementById("deviceCaption");
  let cur = 0, timer = null;

  function show(i) {
    cur = (i + imgs.length) % imgs.length;
    imgs.forEach((im, k) => im.classList.toggle("active", k === cur));
    thumbs.forEach((t, k) => t.classList.toggle("active", k === cur));
    caption.textContent = thumbs[cur].dataset.caption;
  }
  thumbs.forEach((t) => t.addEventListener("click", () => {
    show(+t.dataset.i);
    clearInterval(timer); timer = setInterval(() => show(cur + 1), 5000);
  }));
  const devIO = new IntersectionObserver((en) => {
    if (en[0].isIntersecting) { timer = setInterval(() => show(cur + 1), 5000); devIO.disconnect(); }
  }, { threshold: 0.3 });
  devIO.observe(document.getElementById("device"));

  /* ---------- 7. 终端树打字 ---------- */
  const TREE = [
    { t: "screen_lcd", c: "#3ce0c8", b: true },
    { t: "├─ apps/", c: "#9db4ff" },
    { t: "│  ├─ demo9/           # 远程控制 agent(音量/亮度/闹钟)", c: "#b9c4e8" },
    { t: "│  │  ├─ main.c         # ws + 闹钟双线程入口", c: "#b9c4e8" },
    { t: "│  │  ├─ net/           # 自研 WebSocket 客户端", c: "#b9c4e8" },
    { t: "│  │  └─ ctrl/          # 音量/亮度/闹钟控制", c: "#b9c4e8" },
    { t: "│  └─ demo8/           # 完整 UI: 主页 + 设置页", c: "#b9c4e8" },
    { t: "├─ component/", c: "#9db4ff" },
    { t: "│  ├─ font/             # 中文字体裁剪加载", c: "#b9c4e8" },
    { t: "│  ├─ wifi/             # wpa_manager 驱动移植", c: "#b9c4e8" },
    { t: "│  └─ osal/             # 操作系统抽象层", c: "#b9c4e8" },
    { t: "├─ lvgl-master/         # LVGL v9.6 图形库", c: "#b9c4e8" },
    { t: "├─ platform/", c: "#9db4ff" },
    { t: "│  ├─ x86linux/         # SDL 模拟器移植层", c: "#b9c4e8" },
    { t: "│  └─ t113/             # fb + G2D 加速 + 触摸", c: "#b9c4e8" },
    { t: "├─ server/              # 远程控制中继服务器", c: "#9db4ff" },
    { t: "└─ html/", c: "#9db4ff" },
    { t: "   ├─ server.py         # ← 你正在看的这个后端", c: "#a8f0e2" },
    { t: "   └─ index.html        # ← 你正在看的这个页面", c: "#a8f0e2" },
  ];
  const termBody = document.getElementById("terminalBody");
  if (termBody) {
    let li = 0, ch = 0;
    const treeIO = new IntersectionObserver((en) => {
      if (!en[0].isIntersecting) return;
      treeIO.disconnect();
      (function typeTree() {
        if (li >= TREE.length) { ch = 0; li = 0; return; }
        const line = TREE[li];
        ch++;
        const prefix = line.b ? "" : "  ";
        const done = TREE.slice(0, li).map((l) => prefix + l.t).join("\n");
        const partial = done + "\n" + prefix + line.t.slice(0, ch);
        termBody.innerHTML = partial.split("\n").map((ln, k) =>
          k < TREE.length
            ? `<span style="color:${TREE[k].c}">${ln}</span>`
            : ln
        ).join("\n");
        if (ch >= line.t.length) { li++; ch = 0; setTimeout(typeTree, 120); }
        else setTimeout(typeTree, 14);
      })();
    }, { threshold: 0.3 });
    treeIO.observe(termBody);
  }

  /* ---------- 8. 后端 API ---------- */
  const apiBody = document.getElementById("apiBody");
  if (apiBody) {
    fetch("/api/project")
      .then((r) => r.json())
      .then((d) => {
        apiBody.textContent = JSON.stringify(d, null, 2);
        /* 语法高亮: 键 + 字符串 + 数字 */
        apiBody.innerHTML = apiBody.textContent.replace(
          /("(?:[^"\\]|\\.)*")(\s*:)?(\s*("(?:[^"\\]|\\.)*"|-?\d+(?:\.\d+)?|\{|\}|\[|\]))?/g,
          (m, k, c, v) => {
            let out = "";
            if (k) out += `<span style="color:#9db4ff">${k}</span>`;
            if (c) out += c;
            if (v && v.startsWith('"')) out += `<span style="color:#7cf3e2">${v}</span>`;
            else if (v && /^-?\d/.test(v)) out += `<span style="color:#ff9ec7">${v}</span>`;
            else if (v) out += `<span style="color:#c9d4ff">${v}</span>`;
            return out;
          }
        );
      })
      .catch(() => {
        /* file:// 直接打开时无后端: 给一个友好的说明 + 静态快照, 不影响页面其余功能 */
        apiBody.innerHTML =
          '<span style="color:#9db4ff">"note"</span>: <span style="color:#7cf3e2">"当前为直接打开模式 — 数据为静态快照"</span>,\n' +
          '<span style="color:#9db4ff">"hint"</span>: <span style="color:#7cf3e2">"运行 python3 html/server.py 即可看到实时项目统计(真实扫描代码)"</span>,\n' +
          '<span style="color:#9db4ff">"screen"</span>: {<span style="color:#9db4ff">"width"</span>: <span style="color:#ff9ec7">1424</span>, <span style="color:#9db4ff">"height"</span>: <span style="color:#ff9ec7">280</span>},\n' +
          '<span style="color:#9db4ff">"ui"</span>: <span style="color:#7cf3e2">"LVGL v9.6"</span>, <span style="color:#9db4ff">"lang"</span>: <span style="color:#7cf3e2">"C"</span>';
      });
  }
})();
