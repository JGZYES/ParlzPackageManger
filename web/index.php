<?php
/* PMM home — hero + rotating dotted Earth + entry cards to the 4 pages. */
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';

/* Coarse world map, '#'=land — reused as the dot cloud on the globe. */
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
$COLS = 0;
foreach ($map as $row) $COLS = max($COLS, strlen($row));
for ($r = 0; $r < $ROWS; $r++) $map[$r] = str_pad($map[$r], $COLS);
if ($COLS === 0) $COLS = 1;
$dots = array();
for ($r = 0; $r < $ROWS; $r++)
    for ($c = 0; $c < $COLS; $c++)
        if ($map[$r][$c] === '#') $dots[] = array($r, $c);

pmm_header('home', '跨平台 C 语言包管理器');
?>

<header id="top" class="hero">
  <div class="hero-inner">
    <div class="hero-copy">
      <div class="hero-badge">v<?php echo PMM_VERSION; ?> · C11 · Windows / Linux / macOS</div>
      <h1>ParlzPackageManager <span class="accent">(PMM)</span></h1>
      <p class="hero-tag">一个用 <strong>C 语言</strong>从零编写的跨平台包管理器。像 apt 一样多用镜像源，也能直接从 <strong>GitHub / GitLab / Gitea / Forgejo</strong> Release 装包。</p>
      <div class="hero-cta">
        <a class="btn btn-primary" href="install.php">快速安装</a>
        <a class="btn btn-ghost" href="download.php">下载 v<?php echo PMM_VERSION; ?> ↓</a>
      </div>
      <div class="hero-note"><span class="dot ok"></span> 双镜像 · 深圳 / 香港 实时在线</div>
    </div>
    <div class="hero-globe">
      <canvas id="globe" aria-label="PMM 服务分布地球"></canvas>
      <div class="globe-caption">全球镜像 · 深圳 · 香港</div>
    </div>
  </div>
</header>

<section class="section entry-section">
  <div class="wrap">
    <div class="entry-grid">
      <a class="entry-card" href="features.php">
        <span class="entry-icon">⌘</span>
        <span class="entry-title">特性</span>
        <span class="entry-desc">apt 式多镜像、git release、.pdm、依赖解析、并发下载</span>
      </a>
      <a class="entry-card" href="install.php">
        <span class="entry-icon">▸</span>
        <span class="entry-title">安装</span>
        <span class="entry-desc">三条命令装好 Windows / Linux / macOS 客户端</span>
      </a>
      <a class="entry-card" href="download.php">
        <span class="entry-icon">↓</span>
        <span class="entry-title">下载</span>
        <span class="entry-desc">zip / deb / install.sh / .pdm 全部安装产物</span>
      </a>
      <a class="entry-card" href="servers.php">
        <span class="entry-icon">◉</span>
        <span class="entry-title">服务状态</span>
        <span class="entry-desc">深圳 / 香港镜像的实时探测与详细信息</span>
      </a>
    </div>
  </div>
</section>

<p class="tips" style="padding-bottom:40px"><code>pmm install nodejs</code> · <code>pmm install php</code> · <code>pmm install pmm</code>（升级自身）</p>

<script>
window.PMM_MAP = <?php echo json_encode($dots); ?>;
window.PMM_DIMS = { rows: <?php echo $ROWS; ?>, cols: <?php echo $COLS; ?> };
</script>
<?php pmm_footer(); ?>
