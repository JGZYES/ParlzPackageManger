<?php
/* Shared layout + constants for the PMM site (4 pages + home).
 * Pure grayscale dark theme. Each page: define('PMM_SITE',1); require _common.php;
 * then pmm_header($active,$title); ... pmm_footer(); */
if (!defined('PMM_SITE')) { http_response_code(403); exit('forbidden'); }

define('PMM_VERSION', '0.3.5');

function pmm_nav_links(string $active): string {
    $items = [
        'home'     => ['index.php',    '首页'],
        'features' => ['features.php', '特性'],
        'install'  => ['install.php',  '安装'],
        'download' => ['download.php', '下载'],
        'status'   => ['servers.php',  '服务状态'],
        'source'   => ['git.php',   '源码'],
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
'<link rel="icon" href="icon.png">' . "\n" .
'<link rel="apple-touch-icon" href="icon.png">' . "\n" .
'<link rel="stylesheet" href="assets/style.css">' . "\n" .
'</head>' . "\n" .
'<body>' . "\n" .
'<nav class="nav">' . "\n" .
'  <a class="brand" href="index.php"><img class="brand-img" src="icon.png" alt="PMM"><span class="brand-name">ParlzPackageManager</span></a>' . "\n" .
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
