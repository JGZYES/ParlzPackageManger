<?php
/**
 * PMM mirror – minimal file/folder list (tree). No fancy UI: just show the
 * folders and the files under them, indented, clickable to download.
 */
$root = __DIR__;

function human_size($b) {
    $b = (int)$b; $u = array('B','KB','MB','GB'); $i = 0;
    while ($b >= 1024 && $i < 3) { $b /= 1024; $i++; }
    return round($b, 2) . ' ' . $u[$i];
}
function en($s) { return htmlspecialchars($s, ENT_QUOTES, 'UTF-8'); }

$lines = array();
function walk($dir, $rel, $depth, &$lines) {
    $items = scandir($dir);
    // dirs first, then files, each sorted
    $dirs = array(); $files = array();
    foreach ($items as $e) {
        if ($e === '.' || $e === '..' || $e[0] === '.' || $e === 'index.php') continue;
        if (is_dir($dir . '/' . $e)) $dirs[] = $e; else $files[] = $e;
    }
    sort($dirs); sort($files);
    foreach ($dirs as $e) {
        $lines[] = str_repeat('  ', $depth) . '📁 ' . en($e) . '/';
        walk($dir . '/' . $e, $rel . '/' . $e, $depth + 1, $lines);
    }
    foreach ($files as $e) {
        $p = $dir . '/' . $e;
        $lines[] = str_repeat('  ', $depth)
                 . '<a href="' . en($rel . '/' . $e) . '">' . en($e) . '</a>'
                 . '  <span style="color:#888">(' . human_size(filesize($p)) . ')</span>';
    }
}
walk($root, '', 0, $lines);

header('Content-Type: text/html; charset=utf-8');
echo "<!DOCTYPE html><html><head><meta charset='utf-8'><title>PMM 镜像</title>"
   . "<style>body{font-family:Consolas,Menlo,monospace;background:#fff;color:#000;margin:12px} a{color:#00f;text-decoration:none} a:hover{text-decoration:underline}</style></head><body>";
echo "<pre>\n" . implode("\n", $lines) . "\n</pre></body></html>";
