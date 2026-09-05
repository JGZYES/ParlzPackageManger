<?php
/* PMM docs site — scans md/*.md into a left file tree and renders the selected
 * page on the right. URL: docs/index.php?page=<slug> (defaults to "index"). */
define('PMM_SITE', 1);
require dirname(__DIR__) . '/_common.php';

$mdDir = __DIR__ . '/md';

/* Discover every .md file; title = first "# " line, else the filename. */
$pages = [];   // slug -> title
if (is_dir($mdDir)) {
    foreach (glob($mdDir . '/*.md') as $f) {
        $slug = basename($f, '.md');
        $title = $slug;
        $raw = (string)file_get_contents($f);
        if (preg_match('/^#\s+(.+)$/m', $raw, $m)) $title = trim($m[1]);
        $pages[$slug] = $title;
    }
}

$page = isset($_GET['page']) ? (string)$_GET['page'] : 'index';
if (!isset($pages[$page])) $page = 'index';
$title = $pages[$page];

$body = '';
$f = $mdDir . '/' . $page . '.md';
if (is_file($f)) {
    $body = (string)file_get_contents($f);
    $body = str_replace("\r", "", $body);   // md() regex hard-codes \n
    $body = md($body);
}

pmm_header('docs', '文档 · ' . $title);
?>
<style>
/* docs layout — inline, so it works regardless of external style.css state */
.docs-layout{display:grid;grid-template-columns:240px minmax(0,1fr);gap:28px;max-width:1160px;margin:0 auto;padding:28px 22px 60px;align-items:start}
.docs-sidebar{position:sticky;top:14px;min-width:0;border-right:1px solid #282828;padding-right:16px}
.docs-sidebar-brand{font-weight:700;font-size:15px;color:#fff;padding:0 4px 12px;border-bottom:1px solid #282828;margin-bottom:8px}
.docs-tree{display:block}
.docs-tree a.docs-link{display:block;font-size:14px;color:#9a9a9a;text-decoration:none;padding:7px 10px;border-radius:7px;margin:0}
.docs-tree a.docs-link:hover{color:#fff;background:#101010}
.docs-tree a.docs-link.active{color:#000;background:#f2f2f2}
.docs-content{min-width:0}
.docs-crumb{font-size:12px;color:#555;margin-bottom:18px}
.docs-body{max-width:820px;line-height:1.85;color:#ededed;font-size:15px}
.docs-body h1{font-size:30px;margin:0 0 18px;color:#fff;border-bottom:1px solid #282828;padding-bottom:14px}
.docs-body h2{font-size:21px;margin:36px 0 12px;color:#fff}
.docs-body h3{font-size:17px;margin:26px 0 10px;color:#fff}
.docs-body p{margin:0 0 14px}
.docs-body ul,.docs-body ol{padding-left:22px;margin:0 0 14px}
.docs-body li{margin:5px 0}
.docs-body a{color:#fff;text-decoration:underline}
.docs-body a:hover{color:#f2f2f2}
.docs-body strong{color:#fff}
.docs-body code{background:#101010;border:1px solid #282828;padding:1px 6px;border-radius:5px;font-size:13px;font-family:ui-monospace,monospace;color:#9a9a9a}
.docs-body pre.mdcode{background:#101010;border:1px solid #282828;border-radius:10px;padding:14px 16px;overflow:auto;margin:0 0 14px}
.docs-body pre.mdcode code{background:none;border:0;padding:0;color:#ededed;font-size:13px;line-height:1.6;display:block}
.docs-body hr{border:0;border-top:1px solid #282828;margin:22px 0}
.docs-body table{border-collapse:collapse;width:100%;margin:0 0 16px}
.docs-body th,.docs-body td{border:1px solid #282828;padding:8px 12px;font-size:13px;text-align:left}
.docs-body th{background:#101010;color:#fff}
@media(max-width:840px){.docs-layout{grid-template-columns:1fr}.docs-sidebar{position:static;border-right:0;padding-right:0}}
</style>
<section class="docs-layout">
  <aside class="docs-sidebar">
    <div class="docs-sidebar-brand"><span>PMM 文档</span></div>
    <nav class="docs-tree">
      <?php foreach ($pages as $slug => $t): ?>
        <a class="docs-link<?php echo $slug === $page ? ' active' : ''; ?>" href="index.php?page=<?php echo $slug; ?>"><?php echo htmlspecialchars($t); ?></a>
      <?php endforeach; ?>
    </nav>
  </aside>
  <div class="docs-content">
    <div class="docs-crumb">文档 / <?php echo htmlspecialchars($title); ?></div>
    <div class="docs-body"><?php echo $body; ?></div>
  </div>
</section>
<?php pmm_footer(); ?>
