<?php
/* PMM docs site — recursive collapsible file tree (left) + rendered md (right).
 * Scans md/ recursively; folders → <details><summary>, .md → file links.
 * URL: docs/index.php?page=<relpath-without-.md> (defaults to "index"). */
define('PMM_SITE', 1);
require dirname(__DIR__) . '/_common.php';

$mdDir = __DIR__ . '/md';
$page  = isset($_GET['page']) ? (string)$_GET['page'] : 'index';

/* Recursively render the tree under `dir` (relative to mdDir). */
function render_tree(string $dir, string $rel): string {
    $html = '';
    $items = scandir($dir);
    if (!$items) return '';
    $folders = []; $files = [];
    foreach ($items as $it) {
        if ($it === '.' || $it === '..') continue;
        $full = $dir . '/' . $it;
        if (is_dir($full)) $folders[] = $it;
        elseif (substr($it, -3) === '.md') $files[] = $it;
    }
    sort($folders); sort($files);
    foreach ($folders as $f) {
        $childRel = $rel === '' ? $f : $rel . '/' . $f;
        $html .= '<details class="docs-folder"><summary class="docs-folder-name">📁 ' .
                 htmlspecialchars($f) . '</summary><div class="docs-children">' .
                 render_tree($dir . '/' . $f, $childRel) . '</div></details>';
    }
    foreach ($files as $fname) {
        $slug = $rel === '' ? basename($fname, '.md') : $rel . '/' . basename($fname, '.md');
        $title = basename($fname, '.md');
        $raw = (string)@file_get_contents($dir . '/' . $fname);
        if (preg_match('/^#\s+(.+)$/m', $raw, $m)) $title = trim($m[1]);
        $active = ($slug === $GLOBALS['page']) ? ' active' : '';
        $html .= '<a class="docs-link' . $active . '" href="index.php?page=' .
                 rawurlencode($slug) . '">📄 ' . htmlspecialchars($title) . '</a>';
    }
    return $html;
}

$body = '';
$f = $mdDir . '/' . $page . '.md';
if (is_file($f)) {
    $raw = (string)file_get_contents($f);
    $raw = str_replace("\r", "", $raw);
    $body = md($raw);
}
// crumb title = file's first heading or slug
$title = $page;
if ($body !== '' && preg_match('/^#\s+(.+)$/m', str_replace('<', '', $body), $m)) $title = trim(strip_tags($m[1]));

if (isset($_GET['ajax'])) {   /* partial response for AJAX nav (no layout) */
    echo '<div class="docs-crumb">文档 / ' . htmlspecialchars($title) . '</div>';
    echo '<div class="docs-body" id="docs-body">' . $body . '</div>';
    exit;
}

pmm_header('docs', '文档 · ' . $title);
?>
<style>
/* docs layout — inline, works regardless of external CSS */
.docs-layout{display:grid;grid-template-columns:280px minmax(0,1fr);gap:28px;max-width:1160px;margin:0 auto;padding:28px 22px 60px;align-items:start}
.docs-sidebar{position:sticky;top:14px;min-width:0;border-right:1px solid #282828;padding-right:16px}
.docs-sidebar-brand{font-weight:700;font-size:15px;color:#fff;padding:0 4px 12px;border-bottom:1px solid #282828;margin-bottom:8px}
.docs-tree{display:block;font-family:ui-monospace,monospace}
.docs-folder{display:block;margin:2px 0}
.docs-folder[open]>.docs-folder-name{color:#fff}
.docs-folder-name{cursor:pointer;display:block;font-size:13.5px;color:#9a9a9a;padding:5px 6px;border-radius:6px;list-style:none}
.docs-folder-name:hover{color:#fff;background:#101010}
.docs-folder-name::marker{content:""}
.docs-children{margin-left:16px;border-left:1px solid #282828;padding-left:8px}
.docs-tree a.docs-link{display:block;font-size:13.5px;color:#9a9a9a;text-decoration:none;padding:4px 6px;border-radius:6px;margin:0}
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
    <nav class="docs-tree"><?php echo render_tree($mdDir, ''); ?></nav>
  </aside>
  <div class="docs-content">
    <div class="docs-crumb">文档 / <?php echo htmlspecialchars($title); ?></div>
    <div class="docs-body" id="docs-body"><?php echo $body; ?></div>
  </div>
</section>
<script>
(function () {
  var body = document.getElementById('docs-body');
  var content = document.querySelector('.docs-content');
  var crumb = document.querySelector('.docs-crumb');
  if (!body) return;

  // Intercept tree links: fetch the partial (ajax=1) and swap the content area.
  document.querySelectorAll('.docs-tree a.docs-link').forEach(function (a) {
    a.addEventListener('click', function (e) {
      e.preventDefault();
      var url = a.getAttribute('href');
      var m = url.match(/[?&]page=([^&]+)/);
      var page = m ? decodeURIComponent(m[1]) : '';
      document.querySelectorAll('.docs-tree a.docs-link').forEach(function (x) { x.classList.remove('active'); });
      a.classList.add('active');
      fetch('index.php?ajax=1&page=' + encodeURIComponent(page), { cache: 'no-store' })
        .then(function (r) { return r.text(); })
        .then(function (html) {
          content.innerHTML = html;   // includes .docs-crumb + .docs-body
          body = content.querySelector('#docs-body');
          crumb = content.querySelector('.docs-crumb');
          content.querySelectorAll('.docs-body a').forEach(function (x) {
            // internal doc links also AJAX-swap
          });
          window.scrollTo(0, 0);
          if (history && history.pushState) history.pushState({ page: page }, '', 'index.php?page=' + encodeURIComponent(page));
        });
    });
  });
  window.addEventListener('popstate', function (e) {
    var p = (e.state && e.state.page) ? e.state.page : 'index';
    fetch('index.php?ajax=1&page=' + encodeURIComponent(p), { cache: 'no-store' })
      .then(function (r) { return r.text(); })
      .then(function (html) {
        content.innerHTML = html;
        body = content.querySelector('#docs-body');
        crumb = content.querySelector('.docs-crumb');
      });
  });
})();
</script>
<?php pmm_footer(); ?>
