<?php
/* Shared layout + constants for the PMM site (4 pages + home).
 * Pure grayscale dark theme. Each page: define('PMM_SITE',1); require _common.php;
 * then pmm_header($active,$title); ... pmm_footer(); */
if (!defined('PMM_SITE')) { http_response_code(403); exit('forbidden'); }

define('PMM_VERSION', '0.2.9');

function pmm_nav_links(string $active): string {
    $items = [
        'home'     => ['index.php',    '首页'],
        'features' => ['features.php', '特性'],
        'install'  => ['install.php',  '安装'],
        'download' => ['download.php', '下载'],
        'status'   => ['servers.php',  '服务状态'],
    ];
    $html = '';
    foreach ($items as $key => [$href, $label]) {
        $cls = $key === $active ? ' class="active"' : '';
        $html .= '<li><a' . $cls . ' href="' . $href . '">' . $label . '</a></li>';
    }
    return $html;
}

function pmm_header(string $active, string $title): void {
    $t = htmlspecialchars($title, ENT_QUOTES, 'UTF-8');
    echo '<!DOCTYPE html>' . "\n" .
'<html lang="zh-CN">' . "\n" .
'<head>' . "\n" .
'<meta charset="UTF-8">' . "\n" .
'<meta name="viewport" content="width=device-width, initial-scale=1.0">' . "\n" .
'<title>' . $t . ' · ParlzPackageManager</title>' . "\n" .
'<meta name="description" content="ParlzPackageManager (PMM) — 用 C 语言编写、跨 Windows/Linux/macOS 的包管理器。">' . "\n" .
'<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns=\'http://www.w3.org/2000/svg\' viewBox=\'0 0 32 32\'%3E%3Crect width=\'32\' height=\'32\' rx=\'7\' fill=\'%23050505\'/%3E%3Ctext x=\'16\' y=\'23\' font-size=\'17\' font-family=\'monospace\' fill=\'%23cfcfcf\' text-anchor=\'middle\'%3Epmm%3C/text%3E%3C/svg%3E">' . "\n" .
'<link rel="stylesheet" href="assets/style.css">' . "\n" .
'</head>' . "\n" .
'<body>' . "\n" .
'<nav class="nav">' . "\n" .
'  <a class="brand" href="index.php"><span class="brand-mark">pmm</span><span class="brand-name">ParlzPackageManager</span></a>' . "\n" .
'  <ul class="nav-links">' . "\n" .
'    ' . pmm_nav_links($active) . "\n" .
'    <li><a class="btn btn-sm" href="https://github.com/JGZYES/ParlzPackageManger" target="_blank" rel="noopener">GitHub ↗</a></li>' . "\n" .
'  </ul>' . "\n" .
'</nav>' . "\n";
}

function pmm_page_open(string $kicker, string $title): void {
    $k = htmlspecialchars($kicker, ENT_QUOTES, 'UTF-8');
    $t = htmlspecialchars($title, ENT_QUOTES, 'UTF-8');
    echo '<header class="page-hero"><div class="wrap">' . "\n" .
         '  <div class="page-kicker">' . $k . '</div>' . "\n" .
         '  <h1>' . $t . '</h1>' . "\n" .
         '</div></header>' . "\n";
}

function pmm_footer(): void {
    echo '<footer class="footer"><div class="wrap">' . "\n" .
         '  <div class="footer-top"><span class="brand-name">ParlzPackageManager</span><span>GNU GPL-3.0 · C11 · 跨平台</span></div>' . "\n" .
         '  <div class="footer-links">' . "\n" .
         '    <a href="https://github.com/JGZYES/ParlzPackageManger" target="_blank" rel="noopener">GitHub</a>' . "\n" .
         '    <a href="https://github.com/JGZYES/ParlzPackageManger/releases" target="_blank" rel="noopener">Releases</a>' . "\n" .
         '    <a href="index.php">首页</a>' . "\n" .
         '  </div>' . "\n" .
         '  <p class="footer-note">© 2026 ParlzPackageManager Project · 深圳 sz.pmm.parlz.com · 香港 pmm.parlz.com</p>' . "\n" .
         '</div></footer>' . "\n" .
         '<script src="assets/main.js"></script>' . "\n" .
         '</body>' . "\n" . '</html>';
}
