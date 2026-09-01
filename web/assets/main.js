/* PMM site — world map island glow + install tabs + copy buttons */
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
        try { document.execCommand("copy"); flash("已复制"); }
        catch (e) { flash("复制失败"); }
        document.body.removeChild(ta);
      }
      function flash(msg) {
        btn.textContent = msg; btn.classList.add("ok");
        setTimeout(function () { btn.textContent = "复制"; btn.classList.remove("ok"); }, 1200);
      }
    });
  });

  /* ---------- World map: flood-fill island glow ---------- */
  var mapEl = document.getElementById("worldmap");
  if (!mapEl) return;
  var rows = mapEl.querySelectorAll(".row");
  var grid = [];
  var R = grid.length, C = 0;
  rows.forEach(function (rowEl) {
    var arr = Array.prototype.slice.call(rowEl.children);
    grid.push(arr);
    C = Math.max(C, arr.length);
  });
  R = grid.length;

  function isLand(r, c) {
    if (r < 0 || r >= R || c < 0 || c >= C) return false;
    var el = grid[r][c];
    return el && el.classList.contains("land");
  }

  function clearHot() {
    mapEl.querySelectorAll(".px.hot,.px.hot2").forEach(function (el) {
      el.classList.remove("hot", "hot2");
      el.style.transitionDelay = "";
    });
  }

  function light(r, c) {
    clearHot();
    if (!isLand(r, c)) return;
    // BFS over the island (8-connectivity), staggered outward for a "flood" look
    var seen = {}, queue = [[r, c, 0]];
    seen[r + "," + c] = 1;
    var dirs = [[-1,-1],[-1,0],[-1,1],[0,-1],[0,1],[1,-1],[1,0],[1,1]];
    while (queue.length) {
      var cell = queue.shift();
      var cr = cell[0], cc = cell[1], d = cell[2];
      var el = grid[cr][cc];
      var hot = d % 3 === 1 ? "hot2" : "hot";
      el.style.transitionDelay = Math.min(d * 12, 320) + "ms";
      el.classList.remove("hot", "hot2");
      el.classList.add(hot);
      for (var i = 0; i < dirs.length; i++) {
        var nr = cr + dirs[i][0], nc = cc + dirs[i][1];
        var k = nr + "," + nc;
        if (isLand(nr, nc) && !seen[k]) { seen[k] = 1; queue.push([nr, nc, d + 1]); }
      }
    }
  }

  mapEl.addEventListener("mouseover", function (e) {
    var el = e.target;
    if (el && el.classList && el.classList.contains("land")) {
      light(parseInt(el.getAttribute("data-r"), 10), parseInt(el.getAttribute("data-c"), 10));
    }
  });
  mapEl.addEventListener("mouseleave", clearHot);
})();
