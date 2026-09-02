/* PMM site — rotating dotted Earth globe + detailed live status + tabs + copy */
(function () {
  "use strict";

  /* ---------- Install platform tabs ---------- */
  document.querySelectorAll("[data-tabs]").forEach(function (tb) {
    tb.querySelectorAll(".tab").forEach(function (tab) {
      tab.addEventListener("click", function () {
        tb.querySelectorAll(".tab").forEach(function (t) { t.classList.remove("active"); });
        tab.classList.add("active");
        var key = tab.getAttribute("data-tab");
        document.querySelectorAll("[data-pane]").forEach(function (p) {
          if (p.getAttribute("data-pane") === key) p.setAttribute("data-active", "1");
          else p.removeAttribute("data-active");
        });
      });
    });
  });

  /* ---------- Copy command buttons ---------- */
  document.querySelectorAll(".copy-btn").forEach(function (btn) {
    btn.addEventListener("click", function () {
      var txt = btn.getAttribute("data-copy-text");
      if (!txt) {
        var code = document.querySelector('[data-pane="' + btn.getAttribute("data-copy") + '"] code');
        if (!code) return;
        txt = code.innerText;
      }
      (navigator.clipboard && navigator.clipboard.writeText)
        ? navigator.clipboard.writeText(txt).then(ok, fail)
        : fallback(txt);
      function ok() { flash("已复制"); }
      function fail() { fallback(txt); }
      function fallback(t) {
        var ta = document.createElement("textarea");
        ta.value = t; ta.style.position = "fixed"; ta.style.opacity = "0";
        document.body.appendChild(ta); ta.select();
        try { document.execCommand("copy"); flash("已复制"); } catch (e) { flash("复制失败"); }
        document.body.removeChild(ta);
      }
      function flash(msg) {
        btn.textContent = msg; btn.classList.add("ok");
        setTimeout(function () { btn.textContent = "复制"; btn.classList.remove("ok"); }, 1200);
      }
    });
  });

  /* ---------- Rotating dotted Earth globe ---------- */
  var cv = document.getElementById("globe");
  if (cv && window.PMM_MAP && window.PMM_DIMS) {
    var ctx = cv.getContext("2d");
    var R = window.PMM_DIMS.rows, C = window.PMM_DIMS.cols;
    var DG = Math.PI / 180;
    var pts = window.PMM_MAP.map(function (p) {
      return { lat: (76 - p[0] * (152 / R)) * DG, lon: (-180 + p[1] * (360 / C)) * DG };
    });
    var servers = [
      { lat: 22.54 * DG, lon: 114.06 * DG },
      { lat: 22.32 * DG, lon: 114.17 * DG }
    ];

    var W, H, cx, cy, radius;
    function resize() {
      var dpr = Math.min(window.devicePixelRatio || 1, 2);
      var s = cv.clientWidth || 320;
      W = cv.width = s * dpr; H = cv.height = s * dpr;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      cx = s / 2; cy = s / 2;
      radius = s / 2 - 6;
    }
    resize();
    window.addEventListener("resize", resize);

    var theta = 0, rafId = null;

    function draw(t) {
      theta += 0.0035;
      var cos = Math.cos, sin = Math.sin;
      ctx.clearRect(0, 0, W, H);

      ctx.beginPath();
      ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
      ctx.fillStyle = "rgba(255,255,255,0.045)";
      ctx.fill();
      ctx.strokeStyle = "rgba(255,255,255,0.20)";
      ctx.lineWidth = 1;
      ctx.stroke();

      ctx.strokeStyle = "rgba(255,255,255,0.11)";
      ctx.beginPath(); ctx.ellipse(cx, cy, radius, radius * 0.98, 0, 0, 2 * Math.PI); ctx.stroke();

      function proj(lat, lon) {
        var r = lon + theta;
        return { x: cos(lat) * sin(r), y: sin(lat), z: cos(lat) * cos(r) };
      }

      for (var pass = 0; pass < 2; pass++) {
        var front = pass === 1;
        for (var i = 0; i < pts.length; i++) {
          var q = proj(pts[i].lat, pts[i].lon);
          if ((q.z > 0) !== front) continue;
          ctx.fillStyle = front ? "rgba(230,230,230,0.85)" : "rgba(230,230,230,0.16)";
          ctx.fillRect(cx + q.x * radius, cy - q.y * radius, 1.6, 1.6);
        }
      }

      servers.forEach(function (s) {
        var q = proj(s.lat, s.lon);
        if (q.z <= 0) return;
        var sx = cx + q.x * radius, sy = cy - q.y * radius;
        var pulse = 0.6 + 0.4 * sin(t * 0.006);
        ctx.beginPath();
        ctx.arc(sx, sy, 3, 0, 2 * Math.PI);
        ctx.fillStyle = "#ffffff";
        ctx.fill();
        ctx.beginPath();
        ctx.arc(sx, sy, 4 + (1 - pulse) * 4, 0, 2 * Math.PI);
        ctx.strokeStyle = "rgba(255,255,255," + (0.5 * pulse + 0.15) + ")";
        ctx.lineWidth = 1.5;
        ctx.stroke();
      });

      rafId = requestAnimationFrame(draw);
    }

    if ("IntersectionObserver" in window) {
      var io = new IntersectionObserver(function (en) {
        en.forEach(function (e) {
          if (e.isIntersecting) { if (!rafId) rafId = requestAnimationFrame(draw); }
          else if (rafId) { cancelAnimationFrame(rafId); rafId = null; }
        });
      }, { threshold: 0.1 });
      io.observe(cv);
    } else {
      rafId = requestAnimationFrame(draw);
    }
  }

  /* ---------- Detailed live mirror status (servers.php) ---------- */
  function esc(s) {
    return String(s == null ? "" : s).replace(/[&<>"']/g, function (c) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
    });
  }
  function ms(v) { return (v == null ? "—" : v + " ms"); }

  function renderStatus(data) {
    if (!Array.isArray(data)) return;
    var detail = document.getElementById("svc-detail");
    var html = "";
    data.forEach(function (s, i) {
      var dot = document.getElementById("dot-" + i);
      var meta = document.getElementById("meta-" + i);
      if (dot) dot.className = "status-dot " + (s.online ? "up" : "down");
      if (meta)
        meta.innerHTML = s.online
          ? "在线 · 平均 <b>" + esc(s.avg_ms) + "</b> ms"
          : "离线 / 不可达";
      html += '<div class="svc-card">'
           + '<h3><span class="status-dot ' + (s.online ? "up" : "down") + '"></span>'
           + esc(s.name) + ' · <span style="color:#9a9a9a;font-weight:400">' + esc(s.region) + '</span></h3>'
           + '<div class="svc-host">' + esc(s.host) + ' · 检测时间 ' + esc(s.checked_at) + '</div>'
           + '<table class="svc-table"><tr><th>检测项</th><th>状态</th><th>HTTP</th><th>延迟</th><th>Server</th></tr>';
      (s.checks || []).forEach(function (c) {
        var r = c.result || {};
        html += '<tr><td>' + esc(c.item) + '<br><code>' + esc(c.url) + '</code></td>'
             + '<td><span class="svc-badge ' + (r.ok ? "up" : "down") + '">' + (r.ok ? "正常" : "异常") + '</span></td>'
             + '<td class="' + (r.ok ? "svc-ok" : "svc-bad") + '">' + (r.code || "—") + '</td>'
             + '<td>' + ms(r.ms) + '</td>'
             + '<td><code>' + esc(r.server || "—") + '</code></td></tr>';
      });
      html += '</table></div>';
    });
    if (detail) detail.innerHTML = html;
    var upd = document.getElementById("status-updated");
    if (upd && data[0] && data[0].checked_at) upd.textContent = "最近检测：" + data[0].checked_at + "（UTC）";
  }

  function loadStatus() {
    fetch("status.php", { cache: "no-store" })
      .then(function (r) { return r.json(); })
      .then(renderStatus)
      .catch(function () {});
  }
  var refreshBtn = document.getElementById("status-refresh");
  if (refreshBtn) refreshBtn.addEventListener("click", loadStatus);
  if (document.getElementById("svc-detail")) {
    loadStatus();
    setInterval(loadStatus, 15000);
  }
})();
