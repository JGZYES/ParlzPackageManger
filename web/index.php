<?php
/* PMM home — minimal brand page. All content lives in the docs site. */
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';

pmm_header('home', '跨平台 C 语言包管理器');
?>

<header id="top" class="hero">
  <div class="hero-inner">
    <div class="hero-badge">v<?php echo PMM_VERSION; ?> · C11 · Windows / Linux / macOS</div>
    <h1>ParlzPackageManager <span class="accent">(PMM)</span></h1>
    <p class="hero-tag">一个用 <strong>C 语言</strong>从零编写的跨平台包管理器。像 apt 一样多用镜像源，也能直接从 <strong>GitHub / GitLab / Gitea / Forgejo</strong> Release 装包。</p>
    <div class="hero-cta">
      <a class="btn btn-primary" href="docs/index.php">文档 ▸</a>
      <a class="btn btn-ghost" href="docs/index.php?page=install">安装</a>
      <a class="btn btn-ghost" href="docs/index.php?page=download">下载 v<?php echo PMM_VERSION; ?> ↓</a>
    </div>
    <div class="hero-note"><span class="dot ok"></span> 双镜像 · 深圳 / 香港 实时在线</div>
  </div>
</header>

<section class="section entry-section">
  <div class="wrap">
    <div class="entry-grid">
      <a class="entry-card" href="docs/index.php">
        <span class="entry-icon">▸</span>
        <span class="entry-title">文档</span>
        <span class="entry-desc">特性、命令参考、镜像、翻译</span>
      </a>
      <a class="entry-card" href="docs/index.php?page=install">
        <span class="entry-icon">▸</span>
        <span class="entry-title">安装</span>
        <span class="entry-desc">Windows / Linux / macOS 一键安装</span>
      </a>
      <a class="entry-card" href="docs/index.php?page=download">
        <span class="entry-icon">↓</span>
        <span class="entry-title">下载</span>
        <span class="entry-desc">zip / deb / rpm / install.sh / .pdm</span>
      </a>
    </div>
  </div>
</section>

<p class="tips" style="padding-bottom:40px"><code>pmm install nodejs</code> · <code>pmm install php</code> · <code>pmm install pmm</code>（升级自身）</p>

<?php pmm_footer(); ?>
