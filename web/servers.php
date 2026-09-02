<?php
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';
pmm_header('status', '服务状态');
pmm_page_open('SERVER STATUS', '服务状态');
?>

<section class="section" style="padding-top:24px">
  <div class="wrap">
    <p class="section-sub">实时监控 PMM 双镜像（深圳 / 香港）的注册表、下载服务与 Web 浏览三项能力，每 15 秒自动刷新。不暴露任何服务器 IP。</p>

    <div class="status-toolbar">
      <span class="status-updated" id="status-updated">检测中…</span>
      <button class="btn btn-ghost btn-sm" id="status-refresh" type="button">手动刷新</button>
    </div>

    <div class="svc-grid" id="svc-grid">
      <div class="status-card"><div class="status-head"><span class="pin">深圳</span><span class="status-dot" id="dot-0"></span></div><div class="status-ip">sz.pmm.parlz.com</div><div class="status-host">https://sz.pmm.parlz.com/mirror/packages</div><div class="status-meta" id="meta-0">检测中…</div></div>
      <div class="status-card"><div class="status-head"><span class="pin">香港</span><span class="status-dot" id="dot-1"></span></div><div class="status-ip">pmm.parlz.com</div><div class="status-host">https://pmm.parlz.com/mirror/packages</div><div class="status-meta" id="meta-1">检测中…</div></div>
    </div>

    <div id="svc-detail"></div>
  </div>
</section>

<?php pmm_footer(); ?>
