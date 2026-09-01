<?php
/*
 * ParlzPackageManager (PMM) — official site (dark, pure flat colors).
 * Hero: left = PMM intro, right = rotating dotted Earth globe (canvas).
 * A status panel monitors the SZ + HK mirror servers via status.php.
 * Static PHP, no dependencies.
 */
$VERSION = '0.2.6';

/* Coarse world map, '#'=land, ' '=ocean. Reused as the dot cloud on the globe. */
$map = [
"                                                                                      ",
"                      ####################                                            ",
"                  ##########################                                          ",
"                 #####      #############  ###                                        ",
"                #####        ###### ###      ##                    ####               ",
"                ###           ###   ####  ##  ###              #########              ",
"                ###            ###   ###   ##  ####           ###########             ",
"                ###             ###  ##    ##########          #####  ##              ",
"                 ######          ### ###   ##  ##  ############    ###  ##            ",
"                  #######        ######     ##  ### ########        ##  ##            ",
"                    ###  #        ####     ###   ##########         #### ##           ",
"                     ##  ###        ###    #      ##########         ### ##           ",
"                     ##    ##  #####  ###  ##     #### #####          ## ##           ",
"                     ##     ########     ##  #   ##    ###             ##             ",
"                      #      ##  ####    ##    ##    #                 ## ##          ",
"                             ###  ###    ###    ##                             ##     ",
"                         #    ###   ######  ###  ##                        ##  ##   ",
"                        ###   ##      ####  ##  ##                       ##     ##   ",
"                       ####   ##  ##   ##   ### ##                     ##       ##  ",
"                        ###   ### ###   ##  ###########                ##              ",
"                        ###   #   ##    #    #  #   ### ###           ###               ",
"                        ###     ##     #  ##  #     #####   ##        ###   ##          ",
"                         ##     #       ###    #     ###   ###      ##   # #           ",
"                          ###             ##  ####   ####       ########  #            ",
"                           ### ##           ##  #     #   #     ###  ##  #            ",
"                             ## ##   ##   ### ####  ###     ######## ##               ",
"                               ##    #  #  ##   #  #  ##   ##     ##   ##              ",
"                                #     ##     ##   #       ##   ##      #              ",
"                                       #  ##  #  #  #  ###   #####   ##               ",
"                                         ## ##  #   ##   ##  #   #                     ",
"                                           #   #  #       #  #   #                    ",
"                                               ##  ##  ##    # ##                     ",
"                                                #    #    #   #  #                    ",
"                                                                                      ",
];
$ROWS = count($map);
/* Normalize every row to the same width so $map[$r][$c] never overflows
 * (a shorter row otherwise triggers "Uninitialized string offset" warnings). */
$COLS = 0;
foreach ($map as $row) $COLS = max($COLS, strlen($row));
for ($r = 0; $r < $ROWS; $r++) $map[$r] = str_pad($map[$r], $COLS);
if ($COLS === 0) $COLS = 1;

/* Land cell coordinates for the globe dot cloud */
$dots = array();
for ($r = 0; $r < $ROWS; $r++)
    for ($c = 0; $c < $COLS; $c++)
        if ($map[$r][$c] === '#') $dots[] = array($r, $c);
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ParlzPackageManager · 跨平台 C 语言包管理器</title>
<meta name="description" content="ParlzPackageManager (PMM) — 用 C 语言编写、跨 Windows/Linux/macOS 的包管理器，apt 式多镜像源、git release 安装、.pdm 打包格式、SHA-256 校验。">
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='7' fill='%23050505'/%3E%3Ctext x='16' y='23' font-size='17' font-family='monospace' fill='%23cfcfcf' text-anchor='middle'%3Epmm%3C/text%3E%3C/svg%3E">
<link rel="stylesheet" href="assets/style.css">
</head>
<body>

<nav class="nav">
  <a class="brand" href="#top"><span class="brand-mark">pmm</span><span class="brand-name">ParlzPackageManager</span></a>
  <ul class="nav-links">
    <li><a href="#features">特性</a></li>
    <li><a href="#install">安装</a></li>
    <li><a href="#download">下载</a></li>
    <li><a href="#status">服务状态</a></li>
    <li><a class="btn btn-sm" href="https://github.com/JGZYES/ParlzPackageManger" target="_blank" rel="noopener">GitHub ↗</a></li>
  </ul>
</nav>

<!-- Two-column hero: left intro / right dotted Earth globe -->
<header id="top" class="hero">
  <div class="hero-inner">
    <div class="hero-copy">
      <div class="hero-badge">v<?php echo $VERSION; ?> · C11 · Windows / Linux / macOS</div>
      <h1>ParlzPackageManager <span class="accent">(PMM)</span></h1>
      <p class="hero-tag">一个用 <strong>C 语言</strong>从零编写的跨平台包管理器。像 apt 一样多用镜像源，也能直接从 <strong>GitHub / GitLab / Gitea / Forgejo</strong> Release 装包。</p>
      <div class="hero-cta">
        <a class="btn btn-primary" href="#install">快速开始</a>
        <a class="btn btn-ghost" href="https://github.com/JGZYES/ParlzPackageManger/releases/latest" target="_blank" rel="noopener">最新版 ↓</a>
      </div>
      <div class="hero-note">
        <span class="dot ok"></span> 双镜像 · 深圳 / 香港 实时可用
      </div>
    </div>
    <div class="hero-globe">
      <canvas id="globe" aria-label="PMM 服务分布地球"></canvas>
      <div class="globe-caption">全球镜像 · 深圳 · 香港</div>
    </div>
  </div>
</header>

<section id="status" class="section">
  <div class="wrap">
    <h2 class="section-title">服务 <span class="accent">状态</span></h2>
    <p class="section-sub">实时监控 PMM 镜像服务器可用性与延迟（每 12 秒刷新）。</p>
    <div class="status-grid" id="status-grid">
      <div class="status-card">
        <div class="status-head"><span class="pin">深圳</span><span class="status-dot" id="dot-0"></span></div>
        <div class="status-ip">sz.pmm.parlz.com</div>
        <div class="status-host">https://sz.pmm.parlz.com/mirror/packages</div>
        <div class="status-meta" id="meta-0">检测中…</div>
      </div>
      <div class="status-card">
        <div class="status-head"><span class="pin">香港</span><span class="status-dot" id="dot-1"></span></div>
        <div class="status-ip">pmm.parlz.com</div>
        <div class="status-host">https://pmm.parlz.com/mirror/packages</div>
        <div class="status-meta" id="meta-1">检测中…</div>
      </div>
    </div>
  </div>
</section>

<section id="features" class="section alt">
  <div class="wrap">
    <h2 class="section-title">核心 <span class="accent">特性</span></h2>
    <div class="grid">
      <div class="card"><h3>apt 式多镜像源</h3><p>支持 <code>mirror.ini</code> / <code>mirror.conf</code>，按优先级自动回退；默认带 <code>sz</code>（默认）与 <code>main</code> 两个镜像。</p></div>
      <div class="card"><h3>Git Release 安装</h3><p><code>pmm install --git repo</code> 自动识别 GitHub / GitLab / Gitea / Forgejo 及私有部署，按平台挑资产。</p></div>
      <div class="card"><h3>.pdm 打包格式</h3><p><code>pmm pack &lt;dir&gt;</code> 生成含 <code>control.tar.gz</code> / <code>data.tar.gz</code> / <code>sha256sums</code> 的归档，思路同 .deb。</p></div>
      <div class="card"><h3>哈希校验</h3><p>内置 SHA-256 与 SHA-1 实现，安装前强校验下载文件完整性，防篡改。</p></div>
      <div class="card"><h3>版本选择</h3><p>支持 <code>pmm install pkg==1.2.3</code>、<code>&gt;=</code>、<code>&lt;=</code>、<code>&gt;</code>、<code>&lt;</code> 等 pip 式约束。</p></div>
      <div class="card"><h3>进度条 &amp; 注册表</h3><p>ASCII 下载进度条；Windows 下为已装软件写入注册表卸载项，可用“应用和功能”卸载。</p></div>
    </div>
  </div>
</section>

<section id="install" class="section">
  <div class="wrap">
    <h2 class="section-title">安装 <span class="accent">PMM</span></h2>
    <p class="section-sub">下载对应平台二进制，一条命令搞定，完成后自动加入 PATH。</p>
    <div class="tabs" data-tabs>
      <button class="tab active" data-tab="win">Windows</button>
      <button class="tab" data-tab="linux">Linux</button>
      <button class="tab" data-tab="macos">macOS</button>
    </div>
    <div class="code-box" data-pane="win" data-active="1">
      <pre><code># ZIP 下载（解压即用，pmm.exe 在压缩包内）
curl -L -o pmm.zip https://pmm.parlz.com/downloads/pmm-<?php echo $VERSION; ?>-windows-amd64.zip
# 解压后把 pmm.exe 所在目录加入 PATH，然后：
pmm -v</code></pre>
      <button class="copy-btn" data-copy="win">复制</button>
    </div>
    <div class="code-box" data-pane="linux">
      <pre><code># 方式一：sh 一键安装（自动下载二进制 + 加 PATH）
curl -sSL https://pmm.parlz.com/install.sh | bash

# 方式二：.deb 安装
curl -L -o pmm.deb https://pmm.parlz.com/downloads/pmm_<?php echo $VERSION; ?>_amd64.deb
sudo apt install ./pmm.deb
pmm -v</code></pre>
      <button class="copy-btn" data-copy="linux">复制</button>
    </div>
    <div class="code-box" data-pane="macos">
      <pre><code># sh 一键安装
curl -sSL https://pmm.parlz.com/install.sh | bash
pmm -v</code></pre>
      <button class="copy-btn" data-copy="macos">复制</button>
    </div>
  </div>
</section>

<section id="download" class="section alt">
  <div class="wrap">
    <h2 class="section-title">下载 <span class="accent">PMM</span></h2>
    <p class="section-sub">可从官方镜像站或 GitHub Release 获取。镜像地址不变。</p>
    <div class="dl-grid">
      <a class="dl-card" href="downloads/pmm-<?php echo $VERSION; ?>-windows-amd64.zip" download>
        <span class="dl-os">Windows</span><span class="dl-file">pmm-<?php echo $VERSION; ?>-windows-amd64.zip</span><span class="dl-arrow">↓</span>
      </a>
      <a class="dl-card" href="downloads/pmm_<?php echo $VERSION; ?>_amd64.deb" download>
        <span class="dl-os">Linux</span><span class="dl-file">pmm_<?php echo $VERSION; ?>_amd64.deb</span><span class="dl-arrow">↓</span>
      </a>
      <a class="dl-card" href="install.sh" download>
        <span class="dl-os">Linux/macOS</span><span class="dl-file">install.sh · 一键安装</span><span class="dl-arrow">↓</span>
      </a>
      <a class="dl-card" href="https://github.com/JGZYES/ParlzPackageManger/releases/latest" target="_blank" rel="noopener">
        <span class="dl-os">全部</span><span class="dl-file">GitHub Releases / 源码</span><span class="dl-arrow">↗</span>
      </a>
    </div>
    <p class="tips">Linux 也可 <code>curl -sSL https://pmm.parlz.com/install.sh | bash</code>；<code>.pdm</code> 包仍可从镜像 <code>pmm install pmm</code> / <code>pmm install php</code> 安装。</p>
  </div>
</section>

<section id="mirror" class="section">
  <div class="wrap">
    <h2 class="section-title">镜像 <span class="accent">源</span></h2>
    <p class="section-sub">PMM 默认镜像（apt 式，按优先级回退），地址与 <code>mirror.ini</code> 一致。</p>
    <div class="mirror-list">
      <div class="mirror-item"><span class="mirror-tag default">sz · 默认</span><code>https://sz.pmm.parlz.com/mirror/packages</code></div>
      <div class="mirror-item"><span class="mirror-tag">main · 备份</span><code>https://pmm.parlz.com/mirror/packages</code></div>
    </div>
    <a class="btn btn-ghost" href="mirror/" target="_blank" rel="noopener">浏览镜像目录 ↗</a>
    <p class="tips"><strong>快速开始：</strong><br><code>pmm install nodejs</code> — Node.js<br><code>pmm install php</code> — PHP<br><code>pmm install pmm</code> — 升级 PMM 自身</p>
  </div>
</section>

<footer class="footer">
  <div class="wrap">
    <div class="footer-top"><span class="brand-name">ParlzPackageManager</span><span>GNU GPL-3.0 · C11 · 跨平台</span></div>
    <div class="footer-links">
      <a href="https://github.com/JGZYES/ParlzPackageManger" target="_blank" rel="noopener">GitHub</a>
      <a href="https://github.com/JGZYES/ParlzPackageManger/releases" target="_blank" rel="noopener">Releases</a>
      <a href="#top">回到顶部</a>
    </div>
    <p class="footer-note">© 2026 ParlzPackageManager Project · 深圳 sz.pmm.parlz.com · 香港 pmm.parlz.com</p>
  </div>
</footer>

<script>
window.PMM_MAP = <?php echo json_encode($dots); ?>;
window.PMM_DIMS = { rows: <?php echo $ROWS; ?>, cols: <?php echo $COLS; ?> };
</script>
<script src="assets/main.js"></script>
</body>
</html>
