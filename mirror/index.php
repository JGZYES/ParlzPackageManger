<?php
/**
 * PMM mirror – read-only PHP package browser (like TUNA/tsinghua apt mirrors):
 * lists packages, shows each package's platform variants, and links to the
 * real .pdm downloads. No upload. Compatible with PHP 5.4+.
 *
 *   GET /                      -> package index
 *   GET /?pkg=nodejs           -> nodejs variants + download links
 *   packages/<pkg>/<file>.pdm  -> served as a static file by the web server
 *
 * Deploy:  php -S 0.0.0.0:80  (or Apache/Nginx) serving this directory.
 */
$root = __DIR__;
$pkg  = $root . '/packages';
$base = getenv('PMM_BASE_URL');
if (!$base) $base = 'https://pmm.parlz.com/mirror';
$base = rtrim($base, '/');

function esc($s) { return htmlspecialchars((string)$s, ENT_QUOTES, 'UTF-8'); }

function human_size($bytes) {
    $bytes = (int)$bytes;
    $u = array('B','KB','MB','GB');
    $i = 0;
    while ($bytes >= 1024 && $i < 3) { $bytes /= 1024; $i++; }
    return round($bytes, 2) . ' ' . $u[$i];
}

function pkg_list($dir) {
    $out = array();
    foreach (glob($dir . '/*.json') as $f) {
        $m = json_decode(file_get_contents($f), true);
        if (!is_array($m) || empty($m['variants'])) continue;
        $out[] = $m;
    }
    usort($out, function ($a, $b) { return strcmp($a['name'], $b['name']); });
    return $out;
}

function pkg_meta($dir, $name) {
    $f = $dir . '/' . $name . '.json';
    if (!is_file($f)) return null;
    $m = json_decode(file_get_contents($f), true);
    return (is_array($m) && !empty($m['variants'])) ? $m : null;
}

function pkg_size($dir, $file) { $p = $dir . '/' . $file; return is_file($p) ? filesize($p) : 0; }

function page_head($title) {
    echo "<!DOCTYPE html><html lang='zh-CN'><head><meta charset='utf-8'>"
       . "<meta name='viewport' content='width=device-width,initial-scale=1'>"
       . "<title>" . esc($title) . " · PMM 镜像</title><style>"
       . "body{font-family:system-ui,-apple-system,'Segoe UI','Microsoft YaHei',sans-serif;margin:0;background:#0f172a;color:#e2e8f0}"
       . "header{padding:18px 28px;border-bottom:1px solid #1e293b}header h1{margin:0;font-size:18px}"
       . "header a{color:#38bdf8;text-decoration:none}main{padding:20px 28px;max-width:960px;margin:0 auto}"
       . "table{width:100%;border-collapse:collapse;background:#111827}th,td{padding:10px 12px;text-align:left;border-bottom:1px solid #1f2937;font-size:14px}"
       . "th{background:#0b1220;color:#94a3b8}td a{color:#38bdf8;text-decoration:none}td a:hover{text-decoration:underline}"
       . ".tag{display:inline-block;padding:2px 8px;border-radius:12px;font-size:12px;background:#1e293b;border:1px solid #334155;color:#94a3b8;margin:1px}"
       . ".down{color:#4ade80}.crumb{color:#94a3b8;font-size:13px;margin-bottom:12px}"
       . "footer{color:#64748b;text-align:center;padding:22px;font-size:12px}</style></head><body>";
    echo "<header><h1>📦 PMM 镜像 &mdash; <a href='".esc($base)."/'>".esc($base)."</a></h1></header><main>";
}

function page_foot() {
    echo "</main><footer>ParlzPackageManger · mirrors · <code>pmm install &lt;pkg&gt;</code></footer></body></html>";
}

// ---- router ----
header('Content-Type: text/html; charset=utf-8');
header('Access-Control-Allow-Origin: *');

$pkgName = isset($_GET['pkg']) ? trim((string)$_GET['pkg']) : '';

if ($pkgName !== '') {
    $m = pkg_meta($pkg, $pkgName);
    page_head($pkgName);
    if ($m === null) { echo "<p>没有找到包：".esc($pkgName)."</p>"; page_foot(); exit; }
    echo "<div class='crumb'><a href='".esc($base)."/'>← 返回全部包</a> &nbsp;/&nbsp; <b>".esc($m['name'])."</b> ".(isset($m['description'])?esc($m['description']):'')."</div>";
    echo "<table><tr><th>版本</th><th>平台</th><th>架构</th><th>大小</th><th>sha256</th><th></th></tr>";
    foreach ($m['variants'] as $v) {
        $file = isset($v['file']) ? $v['file'] : ($v['version'].'-'.(isset($v['os'])?$v['os']:'any').'.pdm');
        $size = pkg_size($pkg, $file);
        $sha  = isset($v['sha256']) ? substr($v['sha256'], 0, 16).'…' : '-';
        $href = 'packages/' . rawurlencode($m['name']) . '/' . rawurlencode($file);
        echo "<tr><td>" . esc($v['version']) . "</td><td>" . esc($v['os']) . "</td><td>" . esc(isset($v['arch'])?$v['arch']:'-') . "</td>"
           . "<td>" . human_size($size) . "</td><td title='".esc($v['sha256'])."'>".esc($sha)."</td>"
           . "<td><a class='down' href='".esc($href)."'>下载 .pdm</a></td></tr>";
    }
    echo "</table>";
    page_foot();
    exit;
}

// ---- index ----
page_head('软件包索引');
echo "<div class='crumb'>Index of <b>/mirror</b> · 共 ".count(pkg_list($pkg))." 个包。" .
     " 客户端: <code>pmm install &lt;pkg&gt;</code>，镜像 <code>".esc('registry='.$base.'/packages')."</code></div>";
echo "<table><tr><th>包名</th><th>最新版本</th><th>平台/架构</th><th>安装</th></tr>";
foreach (pkg_list($pkg) as $m) {
    $name = $m['name'];
    $tags = '';
    foreach ($m['variants'] as $v) {
        $tags .= "<span class='tag'>" . esc($v['os']) . (isset($v['arch']) ? '/' . esc($v['arch']) : '') . "</span> ";
    }
    echo "<tr><td><a href='?pkg=" . urlencode($name) . "'>" . esc($name) . "</a></td>"
       . "<td>" . esc($m['version']) . "</td><td>$tags</td>"
       . "<td><code>pmm install ".esc($name)."</code></td></tr>";
}
echo "</table>";
page_foot();
