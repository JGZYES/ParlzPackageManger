<?php
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';
pmm_header('download', '下载');
pmm_page_open('DOWNLOAD', '下载 PMM');
?>

<section class="section">
  <div class="wrap">
    <p class="section-sub">可从官方镜像站或 GitHub Release 获取。镜像地址不变。</p>
    <div class="dl-grid">
      <a class="dl-card" href="downloads/pmm-<?php echo PMM_VERSION; ?>-windows-amd64.zip" download>
        <span class="dl-os">Windows</span><span class="dl-file">pmm-<?php echo PMM_VERSION; ?>-windows-amd64.zip · 免安装</span><span class="dl-arrow">↓</span>
      </a>
      <a class="dl-card" href="downloads/pmm_<?php echo PMM_VERSION; ?>_amd64.deb" download>
        <span class="dl-os">Linux</span><span class="dl-file">pmm_<?php echo PMM_VERSION; ?>_amd64.deb</span><span class="dl-arrow">↓</span>
      </a>
      <a class="dl-card" href="downloads/pmm-<?php echo PMM_VERSION; ?>.x86_64.rpm" download>
        <span class="dl-os">Linux</span><span class="dl-file">pmm-<?php echo PMM_VERSION; ?>.x86_64.rpm</span><span class="dl-arrow">↓</span>
      </a>
      <a class="dl-card" href="install.sh" download>
        <span class="dl-os">Linux/macOS</span><span class="dl-file">install.sh · 一键安装</span><span class="dl-arrow">↓</span>
      </a>
      <a class="dl-card" href="https://github.com/JGZYES/ParlzPackageManger/releases/latest" target="_blank" rel="noopener">
        <span class="dl-os">全部</span><span class="dl-file">GitHub Releases / 源码</span><span class="dl-arrow">↗</span>
      </a>
    </div>
    <p class="tips">Linux 也可 <code>curl -sSL https://pmm.parlz.com/install.sh | bash</code>；<code>.pdm</code> 包可直接 <code>pmm install pmm</code> 安装。</p>

    <h2 class="section-title" style="margin-top:56px">镜像 <span class="accent">源</span></h2>
    <p class="section-sub">PMM 默认镜像（apt 式，按优先级回退），地址与 <code>mirror.ini</code> 一致。</p>
    <div class="mirror-list">
      <div class="mirror-item"><span class="mirror-tag default">sz · 默认</span><code>https://sz.pmm.parlz.com/mirror/packages</code></div>
      <div class="mirror-item"><span class="mirror-tag">main · 备份</span><code>https://pmm.parlz.com/mirror/packages</code></div>
    </div>
    <div style="text-align:center"><a class="btn btn-ghost" href="mirror/" target="_blank" rel="noopener">浏览镜像目录 ↗</a></div>
    <p class="tips">全部历史版本的 <code>.pdm</code> 都在镜像 <code>mirror/packages/pmm/</code> 下。</p>
  </div>
</section>

<?php pmm_footer(); ?>
