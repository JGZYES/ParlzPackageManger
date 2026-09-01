/* PMM site — rotating dotted Earth globe + live status + tabs + copy */
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
      var code = document.querySelector('[data-pane="' + btn.getAttribute("data-copy") + '"] code');
      if (!code) return;
      var txt = code.innerText;
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
    // map cell -> lat/lon
    var pts = window.PMM_MAP.map(function (p) {
      return {
        lat: (76 - p[0] * (152 / R)) * DG,
        lon: (-180 + p[1] * (360 / C)) * DG
      };
    });
    // server locations (Shenzhen ~22.5N/114E, HK ~22.3N/114.2E)
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
    var ACC = "#3ea8ff", OKC = "#2dd4a7";

    function draw(t) {
      theta += 0.0035;
      var cos = Math.cos, sin = Math.sin;
      ctx.clearRect(0, 0, W, H);

      // sphere disc
      ctx.beginPath();
      ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
      ctx.fillStyle = "rgba(62,168,255,0.045)";
      ctx.fill();
      ctx.strokeStyle = "rgba(62,168,255,0.20)";
      ctx.lineWidth = 1;
      ctx.stroke();

      // faint equator + meridian rings for a globe feel
      ctx.strokeStyle = "rgba(62,168,255,0.10)";
      ctx.beginPath(); ctx.ellipse(cx, cy, radius, radius * 0.98, 0, 0, 2 * Math.PI); ctx.stroke();

      function proj(lat, lon) {
        var r = lon + theta;
        var x = cos(lat) * sin(r);
        var y = sin(lat);
        var z = cos(lat) * cos(r);
        return { x: x, y: y, z: z };
      }

      // land dot cloud — back pass (z<0) dim, front pass (z>0) bright
      for (var pass = 0; pass < 2; pass++) {
        var front = pass === 1;
        for (var i = 0; i < pts.length; i++) {
          var q = proj(pts[i].lat, pts[i].lon);
          if ((q.z > 0) !== front) continue;
          ctx.fillStyle = front ? "rgba(62,168,255,0.85)" : "rgba(62,168,255,0.18)";
          ctx.fillRect(cx + q.x * radius, cy - q.y * radius, 1.6, 1.6);
        }
      }

      // server location markers (pulse) — only if on the visible hemisphere
      servers.forEach(function (s) {
        var q = proj(s.lat, s.lon);
        if (q.z <= 0) return;
        var sx = cx + q.x * radius, sy = cy - q.y * radius;
        var pulse = 0.6 + 0.4 * sin(t * 0.006);
        ctx.beginPath();
        ctx.arc(sx, sy, 3, 0, 2 * Math.PI);
        ctx.fillStyle = OKC;
        ctx.fill();
        ctx.beginPath();
        ctx.arc(sx, sy, 4 + (1 - pulse) * 4, 0, 2 * Math.PI);
        ctx.strokeStyle = "rgba(45,212,167," + (0.5 * pulse + 0.15) + ")";
        ctx.lineWidth = 1.5;
        ctx.stroke();
      });

      rafId = requestAnimationFrame(draw);
    }

    // only animate while visible
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

  /* ---------- Live mirror status ---------- */
  function loadStatus() {
    fetch("status.php", { cache: "no-store" })
      .then(function (r) { return r.json(); })
      .then(function (data) {
        if (!Array.isArray(data)) return;
        data.forEach(function (s, i) {
          var dot = document.getElementById("dot-" + i);
          var meta = document.getElementById("meta-" + i);
          if (!dot || !meta) return;
          dot.className = "status-dot " + (s.online ? "up" : "down");
          meta.innerHTML = s.online
            ? "在线 · <b>" + s.ms + "</b> ms · HTTP " + s.code
            : "离线 / 不可达";
        });
      })
      .catch(function () {});
  }
  loadStatus();
  setInterval(loadStatus, 12000);
})();
