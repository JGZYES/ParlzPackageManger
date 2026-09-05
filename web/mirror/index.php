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
/* inline SVG icons (no emoji, no external CDN) */
function ic($type) {
    if ($type === 'folder') { $vb = '0 0 512 512'; $p = 'M0 96C0 60.7 28.7 32 64 32H132.1c19.1 0 37.4 7.6 50.9 21.1L208 78.1 214.1 84H448c35.3 0 64 28.7 64 64v288c0 35.3-28.7 64-64 64H64c-35.3 0-64-28.7-64-64V96z'; }
    else { $vb = '0 0 384 512'; $p = 'M224 136V0H24C10.7 0 0 10.7 0 24v464c0 13.3 10.7 24 24 24h336c13.3 0 24-10.7 24-24V160H248c-13.2 0-24-10.7-24-24zm64 24h-64V96h64v64z'; }
    return '<svg class="ic" width="14" height="14" viewBox="' . $vb . '" aria-hidden="true"><path d="' . $p . '"/></svg>';
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

/* ---------------- collect folder entries ---------------- */
$dirs = array(); $files = array();
foreach (scandir($cur) as $e) {
    if ($e === '.' || $e === '..' || $e[0] === '.' || $e === 'index.php') continue;
    if (strcasecmp($e, 'README.md') === 0) continue; /* do not show README.md */
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
   . " .ic{width:14px;height:14px;fill:currentColor;vertical-align:-2px;margin-right:6px}"
   . " .bar{margin-bottom:12px;font-size:13px}"
   . "</style></head><body>";

echo "<h1>" . ic('folder') . "PMM 镜像" . ($webRel ? " / " . en($webRel) : '') . "</h1>";
echo "<div class='bar'>" . breadcrumb($webRel)
   . " &nbsp;|&nbsp; <a href='?dir=" . ($dirRel === '' ? '' : rawurlencode(dirname($dirRel) === '.' ? '' : dirname($dirRel))) . "'>⬆ 上级</a>"
   . " &nbsp;|&nbsp; <a href='" . ($bg === 'white' ? '?dir=' . rawurlencode($dirRel) : '?dir=' . rawurlencode($dirRel) . '&bg=white') . "'>"
   . ($bg === 'white' ? '◑ 黑底' : '◐ 白底') . "</a></div>";

/* README.md is intentionally NOT shown (removed by request) */

echo "<ul>";
if ($dirRel !== '') {
    $up = dirname($dirRel);
    echo "<li>" . ic('folder') . "<a href='?dir=" . rawurlencode($up === '.' ? '' : $up) . "'><b>..</b></a></li>";
}
foreach ($dirs as $d) {
    $href = '?dir=' . rawurlencode($webRel === '' ? $d : $webRel . '/' . $d);
    echo "<li>" . ic('folder') . "<a href='{$href}'>" . en($d) . "/</a></li>";
}
foreach ($files as $f) {
    $href = url_enc_path($webRel, $f);
    echo "<li>" . ic('file') . "<a href='" . en($href) . "'>" . en($f) . "</a>"
       . "<span class='sz'>(" . human_size(filesize($cur . '/' . $f)) . ")</span></li>";
}
echo "</ul></body></html>";
