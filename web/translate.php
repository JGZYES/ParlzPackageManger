<?php
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';

/* List available .pjson language packs under web/mirror/lang/. */
$langDir = __DIR__ . '/mirror/lang';
$packs = [];
if (is_dir($langDir)) {
    foreach (glob($langDir . '/*.pjson') as $f) {
        $locale = basename($f, '.pjson');
        $keys = 0;
        $data = json_decode((string)file_get_contents($f), true);
        if (is_array($data)) $keys = count($data);
        $packs[] = ['locale' => $locale, 'keys' => $keys, 'file' => 'mirror/lang/' . basename($f)];
    }
}
usort($packs, function ($a, $b) { return strcmp($a['locale'], $b['locale']); });

pmm_header('translate', '翻译 · Translate');
pmm_page_open('TRANSLATE', '翻译 / Language');
?>
<section class="section"><div class="wrap">
  <p>PMM 的界面文案由 <code>.pjson</code> 语言包驱动。默认内置 <b>zh-CN</b> 与 <b>en-US</b>，社区可以提交新的语言包或修正译文。</p>

  <table class="dl-card" style="width:100%; border-collapse:collapse; line-height:1.8;">
    <thead><tr><th style="text-align:left;">Locale</th><th style="text-align:left;">键数</th><th style="text-align:left;">下载</th></tr></thead>
    <tbody>
    <?php if (!$packs): ?>
      <tr><td colspan="3">(暂无语言包)</td></tr>
    <?php else: foreach ($packs as $p): ?>
      <tr>
        <td><code><?php echo htmlspecialchars($p['locale']); ?></code></td>
        <td><?php echo (int)$p['keys']; ?></td>
        <td><a class="copy-btn" href="<?php echo htmlspecialchars($p['file']); ?>" download>下载 .pjson ↓</a></td>
      </tr>
    <?php endforeach; endif; ?>
    </tbody>
  </table>

  <h2>如何贡献翻译</h2>
  <p>复制 <code>zh-CN.pjson</code> 为你的语言（如 <code>ja-JP.pjson</code>），键名保持不变、只改值，然后提交到仓库的
  <code>web/mirror/lang/</code> 目录，或在 GitHub 提交 Issue / PR。社区翻译会即时更新镜像，用户即可通过
  <code>pmm setting lang &lt;locale&gt;</code> 安装。</p>

  <p>安装语言包：</p>
  <pre><code>pmm setting lang ja-JP
pmm setting lang -l</code></pre>
</div></section>
<?php pmm_footer(); ?>
