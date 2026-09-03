<?php
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';
pmm_header('install', '安装');
pmm_page_open('INSTALL', '安装 PMM');
?>

<section class="section">
  <div class="wrap">
    <p class="section-sub">下载对应平台二进制，一条命令搞定，完成后自动加入 PATH。</p>
    <div class="tabs" data-tabs>
      <button class="tab active" data-tab="win">Windows</button>
      <button class="tab" data-tab="linux">Linux</button>
      <button class="tab" data-tab="macos">macOS</button>
    </div>
    <div class="code-box" data-pane="win" data-active="1">
      <pre><code># ZIP 免安装版（解压即用，pmm.exe 在压缩包内）
curl -L -o pmm.zip https://pmm.parlz.com/releases/v<?php echo PMM_VERSION; ?>/pmm-<?php echo PMM_VERSION; ?>-windows-amd64.zip
# 解压后把 pmm.exe 所在目录加入 PATH，然后：
pmm -v</code></pre>
      <button class="copy-btn" data-copy="win">复制</button>
    </div>
    <div class="code-box" data-pane="linux">
      <pre><code># 方式一：sh 一键安装（自动下载二进制 + 加 PATH）
curl -sSL https://pmm.parlz.com/install.sh | bash

# 方式二：.deb 安装
curl -L -o pmm.deb https://pmm.parlz.com/releases/v<?php echo PMM_VERSION; ?>/pmm_<?php echo PMM_VERSION; ?>_amd64.deb
sudo apt install ./pmm.deb

# 方式三：.rpm 安装
curl -L -o pmm.rpm https://pmm.parlz.com/releases/v<?php echo PMM_VERSION; ?>/pmm-<?php echo PMM_VERSION; ?>.x86_64.rpm
sudo rpm -Uvh pmm.rpm
pmm -v</code></pre>
      <button class="copy-btn" data-copy="linux">复制</button>
    </div>
    <div class="code-box" data-pane="macos">
      <pre><code># sh 一键安装
curl -sSL https://pmm.parlz.com/install.sh | bash
pmm -v</code></pre>
      <button class="copy-btn" data-copy="macos">复制</button>
    </div>

    <h2 class="section-title" style="margin-top:56px;font-size:24px">装好之后</h2>
    <div class="code-box">
      <pre><code>pmm search node          # 在镜像里搜包
pmm install nodejs       # 安装（自动解析依赖）
pmm install nodejs==24.20.0   # 指定版本
pmm install --git owner/repo@v1.2.3   # 从 git release 安装指定 tag
pmm list                 # 已装列表
pmm remove nodejs        # 卸载</code></pre>
    </div>
  </div>
</section>

<?php pmm_footer(); ?>
