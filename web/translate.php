<?php
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';

/* Language packs under web/mirror/lang/. */
$langDir = __DIR__ . '/mirror/lang';
$packs = [];
if (is_dir($langDir)) {
    foreach (glob($langDir . '/*.pjson') as $f) {
        $locale = basename($f, '.pjson');
        $keys = 0; $raw = '';
        $data = json_decode((string)file_get_contents($f), true);
        if (is_array($data)) {
            $keys = count($data);
            ksort($data);
            $raw = json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
        }
        $packs[] = [
            'locale' => $locale,
            'keys'   => $keys,
            'file'   => 'mirror/lang/' . basename($f),
            'raw'    => $raw,
        ];
    }
}
usort($packs, function ($a, $b) { return strcmp($a['locale'], $b['locale']); });

$mail = 'luoriguodu@qq.com';
$issues = 'https://github.com/JGZYES/ParlzPackageManger/issues/new';

pmm_header('translate', '翻译 · Translate');
pmm_page_open('TRANSLATE', '翻译 / Language');
?>
<section class="section">
  <div class="wrap">
    <p class="section-sub">PMM 的界面文案由 <code>.pjson</code> 语言包驱动。每门语言一个包；想新增语言或修订译文，请
      <a href="mailto:<?php echo $mail; ?>?subject=<?php echo rawurlencode('[PMM 翻译] 语言包建议'); ?>&body=<?php echo rawurlencode('我想添加/修订语言包：'); ?>">发邮件</a>
      或在 <a href="<?php echo $issues; ?>" target="_blank" rel="noopener">GitHub 提 Issue</a>。</p>

    <div class="dl-grid" style="grid-template-columns:repeat(auto-fill,minmax(260px,1fr));">
      <?php if (!$packs): ?>
        <div class="dl-card"><span class="dl-os">—</span><span class="dl-file">暂无语言包</span></div>
      <?php else: foreach ($packs as $p): ?>
        <div class="dl-card">
          <span class="dl-os"><?php echo htmlspecialchars($p['locale']); ?></span>
          <span class="dl-file"><?php echo (int)$p['keys']; ?> 键</span>

          <details class="lang-view">
            <summary class="btn btn-sm" style="display:inline-block; margin:8px 0 6px;">查看</summary>
            <pre class="code-box" style="max-height:220px; overflow:auto; white-space:pre-wrap; word-break:break-all;"><?php echo htmlspecialchars($p['raw']); ?></pre>
          </details>

          <div style="display:flex; gap:8px; flex-wrap:wrap;">
            <a class="btn btn-sm" href="<?php echo htmlspecialchars($p['file']); ?>" download>下载 .pjson ↓</a>
            <a class="btn btn-sm" href="mailto:<?php echo $mail; ?>?subject=<?php echo rawurlencode('[PMM 翻译] 修订 ' . $p['locale']); ?>&body=<?php echo rawurlencode('针对 ' . $p['locale'] . ' 语言包的建议：'); ?>">贡献修订</a>
          </div>
        </div>
      <?php endforeach; endif; ?>
    </div>

    <h2 class="section-title" style="margin-top:56px">如何贡献一门语言</h2>
    <ol class="tips" style="line-height:1.9;">
      <li>打开任一语言（如 <code>zh-CN</code>）卡片 → <b>下载 .pjson</b>。</li>
      <li>把 <code>zh-CN</code> 的键名保留、<b>只改值</b>,另存为你的语言,如 <code>ja-JP.pjson</code>、<code>zh-TW.pjson</code>。</li>
      <li>将文件作为附件发到 <a href="mailto:<?php echo $mail; ?>"><?php echo $mail; ?></a>,或在 <a href="<?php echo $issues; ?>" target="_blank" rel="noopener">GitHub 提 Issue</a> 粘贴内容。</li>
      <li>通过后即进入镜像 <code>web/mirror/lang/</code>,用户用 <code>pmm setting lang &lt;locale&gt;</code> 安装。</li>
    </ol>

    <p>安装语言包：</p>
    <pre class="code-box"><code>pmm setting lang ja-JP
pmm setting lang -l</code></pre>
  </div>
</section>
<?php pmm_footer(); ?>
