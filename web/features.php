<?php
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';
pmm_header('features', '特性');
pmm_page_open('FEATURES', '核心特性');
?>

<section class="section">
  <div class="wrap">
    <div class="grid">
      <div class="card"><h3>apt 式多镜像源</h3><p>支持 <code>mirror.ini</code> / <code>mirror.conf</code>，按优先级自动回退；默认自带 <code>sz</code>（深圳）与 <code>main</code>（香港）两个镜像，缺失时自动生成。</p></div>
      <div class="card"><h3>Git Release 安装</h3><p><code>pmm install --git repo</code> 自动识别 GitHub / GitLab / Gitea / Forgejo 及私有部署；支持 <code>repo@tag</code> / <code>-b branch</code> 安装指定版本。</p></div>
      <div class="card"><h3>.pdm 打包格式</h3><p><code>pmm pack &lt;dir&gt;</code> 生成含 <code>control.tar.gz</code> / <code>data.tar.gz</code> / <code>sha256sums</code> 的归档，思路同 .deb。</p></div>
      <div class="card"><h3>依赖解析</h3><p><code>pdm-control</code> 声明 <code>Depends:</code>，registry 包声明 <code>"depends": [...]</code>；安装时自动递归解析并按版本约束（==/≥/≤）安装，带防环保护。</p></div>
      <div class="card"><h3>并发下载 + 断点续传</h3><p>大文件（≥8MB）自动 4 路 Range 分片并行下载、逐片续传、拼接后校验；失败自动回退单流。进度条含已用/剩余时间与实时速度。</p></div>
      <div class="card"><h3>哈希校验</h3><p>内置 SHA-256 与 SHA-1（FIPS 180-4），下载后强校验；<code>pmm verify &lt;file&gt;</code> 随时验证任意安装包。</p></div>
      <div class="card"><h3>本地包安装</h3><p><code>pmm install -dpkg x.deb</code>（Linux）、<code>pmm install -msi x.msi</code>（Windows），也支持 <code>.rpm/.zip/.tar.gz/.exe/.dmg</code> 等，直接装文件或 URL。</p></div>
      <div class="card"><h3>搜索与信息</h3><p><code>pmm search &lt;关键词&gt;</code> 发现镜像里的包；<code>pmm info &lt;pkg&gt;</code> 查看版本与平台变体；<code>pmm list</code> 列出已装包。</p></div>
      <div class="card"><h3>缓存管理</h3><p>下载缓存统一放在 <code>&lt;base&gt;/cache</code>；<code>pmm cache clean</code> 一键清空，<code>--no-cache</code> 强制重新下载。</p></div>
    </div>
  </div>
</section>

<?php pmm_footer(); ?>
