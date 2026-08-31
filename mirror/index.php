<?php
/**
 * PMM mirror – simple file browser.
 * Lists every file under this directory (recursively) as a clickable download
 * row; that's all. No package index, no upload. Compatible with PHP 5.4+.
 */
$root = __DIR__;

function human_size($b) {
    $b = (int)$b; $u = array('B','KB','MB','GB'); $i = 0;
    while ($b >= 1024 && $i < 3) { $b /= 1024; $i++; }
    return round($b, 2) . ' ' . $u[$i];
}
function en($s) { return htmlspecialchars($s, ENT_QUOTES, 'UTF-8'); }

$rows = array();
function walk($dir, $rel, &$rows) {
    foreach (scandir($dir) as $e) {
        if ($e === '.' || $e === '..') continue;
        if ($e[0] === '.') continue;              /* hidden files */
        if ($e === 'index.php') continue;          /* this script */
        $p = $dir . '/' . $e;
        if (is_dir($p)) walk($p, $rel . '/' . $e, $rows);
        else $rows[] = array('rel' => $rel . '/' . $e, 'size' => filesize($p));
    }
}
walk($root, '', $rows);
usort($rows, function ($a, $b) { return strcmp($a['rel'], $b['rel']); });

header('Content-Type: text/html; charset=utf-8');
echo "<!DOCTYPE html><html lang='zh-CN'><head><meta charset='utf-8'>"
   . "<meta name='viewport' content='width=device-width,initial-scale=1'>"
   . "<title>PMM 镜像 · 文件列表</title><style>"
   . "body{font-family:system-ui,-apple-system,'Segoe UI','Microsoft YaHei',sans-serif;margin:0;background:#0f172a;color:#e2e8f0}"
   . "header{padding:16px 24px;border-bottom:1px solid #1e293b}header h1{margin:0;font-size:17px}header small{color:#94a3b8}"
   . "main{padding:18px 24px;max-width:920px;margin:0 auto}"
   . "table{width:100%;border-collapse:collapse;background:#111827}th,td{padding:9px 12px;text-align:left;border-bottom:1px solid #1f2937;font-size:14px}"
   . "th{background:#0b1220;color:#94a3b8}td a{color:#38bdf8;text-decoration:none}td a:hover{text-decoration:underline}"
   . "td.sz{color:#94a3b8;width:110px;text-align:right}" . "</style></head><body>"
   . "<header><h1>📁 PMM 镜像 / 文件</h1><small>共 " . count($rows) . " 个文件 · 点击文件名下载</small></header><main>"
   . "<table><tr><th>文件</th><th>大小</th></tr>";
foreach ($rows as $r) {
    echo '<tr><td><a href="' . en($r['rel']) . '">' . en(ltrim($r['rel'], '/')) . '</a></td>'
       . '<td class="sz">' . human_size($r['size']) . '</td></tr>';
}
echo "</table></main></body></html>";
