<?php
/**
 * PMM mirror – minimal GRADED directory browser.
 *   - shows ONE level at a time (folders first, then files, clickable)
 *   - ?dir=<sub> to go into a subfolder; click ".." to go up
 *   - plain white/black background via ?bg=white (default black)
 *   - if the current folder has README.md, it is rendered (basic markdown)
 */
$root = __DIR__;
$rootReal = realpath($root);
$bg = (isset($_GET['bg']) && $_GET['bg'] === 'white') ? 'white' : 'black';

// ---- resolve + sanitize dir ----
$dirRel = isset($_GET['dir']) ? trim((string)$_GET['dir']) : '';
if ($dirRel !== '' && $dirRel !== '/') {
    $dirRel = trim($dirRel, '/');
}
$cur = ($dirRel === '' || $dirRel === '/') ? $rootReal : realpath($root . '/' . $dirRel);
if ($cur === false || strpos($cur, $rootReal) !== 0) { http_response_code(400); die('bad dir'); }
$webRel = ($cur === $rootReal) ? '' : substr(str_replace('\\', '/', $cur), strlen($rootReal) + 1);
$crumbs = $webRel === '' ? array() : explode('/', $webRel);

function en($s) { return htmlspecialchars($s, ENT_QUOTES, 'UTF-8'); }
function human_size($b) {
    $b = (int)$b; $u = array('B','KB','MB','GB'); $i = 0;
    while ($b >= 1024 && $i < 3) { $b /= 1024; $i++; }
    return round($b, 2) . ' ' . $u[$i];
}
function url_enc_path($webRel, $name) {
    $parts = $webRel === '' ? array() : explode('/', $webRel);
    $parts[] = $name;
    $e = array();
    foreach ($parts as $p) $e[] = rawurlencode($p);
    return implode('/', $e);
}
function breadcrumb($rel) {
    $out = '<a href="?">根</a>';
    if ($rel === '') return $out;
    $acc = '';
    foreach (explode('/', $rel) as $i => $part) {
        $acc = $acc === '' ? $part : $acc . '/' . $part;
        $out .= ' / <a href="?dir=' . rawurlencode($acc) . '">' . en($part) . '</a>';
    }
    return $out;
}

/* ---------------- basic markdown renderer ---------------- */
function md_inline($s) {
    $s = en($s);
    $s = preg_replace('/`([^`]+)`/', '<code>$1</code>', $s);
    $s = preg_replace('/\*\*([^*]+)\*\*/', '<b>$1</b>', $s);
    $s = preg_replace('/\[([^\]]+)\]\(([^)]+)\)/', '<a href="$2">$1</a>', $s);
    return $s;
}
function md_render($text) {
    $lines = explode("\n", $text);
    $o = ''; $inList = false;
    foreach ($lines as $line) {
        $line = rtrim($line);
        if ($line === '') { if ($inList) { $o .= "</ul>"; $inList = false; } continue; }
        if (preg_match('/^#{1,6}\s+(.*)$/', $line, $m)) {
            if ($inList) { $o .= "</ul>"; $inList = false; }
            $l = strlen($m[0]) - strlen(preg_replace('/^#+/', '', $m[0]));
            $l = min($l, 6);
            $o .= "<h$l>" . md_inline($m[1]) . "</h$l>";
        } elseif (preg_match('/^\s*[-*]\s+(.*)$/', $line, $m)) {
            if (!$inList) { $o .= "<ul>"; $inList = true; }
            $o .= "<li>" . md_inline($m[1]) . "</li>";
        } elseif (preg_match('/^```/', $line)) {
            if ($inList) { $o .= "</ul>"; $inList = false; }
            $o .= "<pre>" . en(substr($line, 3)) . "</pre>";
        } else {
            if ($inList) { $o .= "</ul>"; $inList = false; }
            $o .= "<p>" . md_inline($line) . "</p>";
        }
    }
    if ($inList) $o .= "</ul>";
    return $o;
}

/* ---------------- collect folder entries ---------------- */
$dirs = array(); $files = array();
foreach (scandir($cur) as $e) {
    if ($e === '.' || $e === '..' || $e[0] === '.' || $e === 'index.php') continue;
    if (is_dir($cur . '/' . $e)) $dirs[] = $e; else $files[] = $e;
}
sort($dirs); sort($files);

$cssDark = 'body{background:#000;color:#e5e5e5} a{color:#7ab8ff}';
$cssWhite = 'body{background:#fff;color:#000} a{color:#0000ee}';
$css = ($bg === 'white') ? $cssWhite : $cssDark;

header('Content-Type: text/html; charset=utf-8');
echo "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
   . "<title>PMM 镜像" . ($webRel ? ' / ' . $webRel : '') . "</title>"
   . "<style>body{font-family:system-ui,'Microsoft YaHei',sans-serif;margin:14px;line-height:1.5}"
   . $css
   . " h1{font-size:16px}a{text-decoration:none}a:hover{text-decoration:underline}code{font-family:monospace}"
   . " ul{list-style:none;padding-left:0}li{margin:3px 0}.sz{color:#888;font-size:12px;margin-left:6px}"
   . " .bar{margin-bottom:12px;font-size:13px}.md{border:1px solid #444;border-radius:6px;padding:0 14px;margin-bottom:16px}"
   . ($bg==='white' ? ' .md{border-color:#ccc}' : '')
   . "</style></head><body>";

echo "<h1>📁 PMM 镜像" . ($webRel ? " / " . en($webRel) : '') . "</h1>";
echo "<div class='bar'>" . breadcrumb($webRel)
   . " &nbsp;|&nbsp; <a href='?dir=" . ($dirRel === '' ? '' : rawurlencode(dirname($dirRel) === '.' ? '' : dirname($dirRel))) . "'>⬆ 上级</a>"
   . " &nbsp;|&nbsp; <a href='" . ($bg === 'white' ? '?dir=' . rawurlencode($dirRel) : '?dir=' . rawurlencode($dirRel) . '&bg=white') . "'>"
   . ($bg === 'white' ? '◑ 黑底' : '◐ 白底') . "</a></div>";

/* README.md in current folder, rendered */
$mdPath = $cur . '/README.md';
if (is_file($mdPath)) {
    echo "<div class='md'>";
    echo md_render(file_get_contents($mdPath));
    echo "</div>";
}

echo "<ul>";
if ($dirRel !== '') {
    $up = dirname($dirRel);
    echo "<li><a href='?dir=" . rawurlencode($up === '.' ? '' : $up) . "'>📁 <b>..</b></a></li>";
}
foreach ($dirs as $d) {
    $href = '?dir=' . rawurlencode($webRel === '' ? $d : $webRel . '/' . $d);
    echo "<li>📁 <a href='${href}'>" . en($d) . "/</a></li>";
}
foreach ($files as $f) {
    $href = url_enc_path($webRel, $f);
    echo "<li>📄 <a href='" . en($href) . "'>" . en($f) . "</a>"
       . "<span class='sz'>(" . human_size(filesize($cur . '/' . $f)) . ")</span></li>";
}
echo "</ul></body></html>";
