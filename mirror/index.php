<?php
/**
 * PMM mirror – pure-PHP registry server (drop-in for the Node server.js).
 *
 * Run with the PHP built-in server:
 *   php -S 0.0.0.0:8080 -t .          # uses this file as router for *.php; add router:
 *   php -S 0.0.0.0:8080 index.php     # simplest: router = this file
 * On Apache/Nginx point all requests to this file (front controller).
 *
 * Endpoints (identical to the Node version):
 *   GET  /                             -> browser UI (index.html)
 *   GET  /api/packages.json, /packages.json  -> index
 *   GET  /mirror.ini                   -> client mirror config
 *   GET  /packages/<pkg>.json          -> latest pointer
 *   GET  /packages/<pkg>/<ver>-<os>.json|.pdm -> per-variant metadata / download
 *   POST /upload                       -> upload a file, write registry variants
 *
 * Storage: packages/<pkg>/<ver>-<os>.pdm, packages/<pkg>/<ver>-<os>.json,
 *          packages/<pkg>.json (latest + variants).
 */

$ROOT = __DIR__;
$PKG  = $ROOT . '/packages';
$BASE = preg_replace('#/+\z#', '', getenv('PMM_BASE_URL') ?: 'https://pmm.parlz.com/mirror');

function base_url() { global $BASE; return $BASE; }
function pkgs_dir() { global $PKG; if (!is_dir($PKG)) mkdir($PKG, 0777, true); return $PKG; }

function send_json($data, $code = 200) {
    http_response_code($code);
    header('Content-Type: application/json');
    echo json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    exit;
}
function send_file($path, $ctype = 'application/octet-stream', $code = 200) {
    if (!is_file($path)) { http_response_code(404); echo 'not found'; exit; }
    http_response_code($code);
    header('Content-Type: ' . $ctype);
    header('Access-Control-Allow-Origin: *');
    header('Content-Length: ' . filesize($path));
    readfile($path);
    exit;
}

function load_index() {
    $out = ['generated' => gmdate('c'), 'packages' => []];
    foreach (glob(pkgs_dir() . '/*.json') as $f) {
        if (is_dir($f)) continue;
        $m = json_decode(file_get_contents($f), true);
        if (!is_array($m) || empty($m['variants'])) continue;
        $out['packages'][] = $m;
    }
    usort($out['packages'], fn($a, $b) => strcmp($a['name'] ?? '', $b['name'] ?? ''));
    return $out;
}

function send_index() { send_json(['generated' => gmdate('c'), 'packages' => load_index()['packages']]); }

function mirror_ini() {
    return "# 由 PMM 镜像站 " . base_url() . " 自动生成\n"
        . "# 复制到 ~/.pmm/mirror.ini 即可 pmm install\n"
        . "[main]\n"
        . "registry = " . base_url() . "/packages\n"
        . "priority = 1\n";
}

function handle_upload() {
    $body = file_get_contents('php://input');
    if ($body === false) { http_response_code(400); echo 'no body'; exit; }
    $H = fn($k) => trim($_SERVER['HTTP_' . strtoupper(str_replace('-', '_', $k))] ?? '');
    $pkg = $H('X-PKG-NAME');
    $ver = $H('X-PKG-VERSION');
    $os  = $H('X-PKG-OS') ?: 'any';
    $desc = $H('X-PKG-DESC') ?: ("Uploaded via PMM mirror (" . base_url() . ")");
    if ($pkg === '') { http_response_code(400); echo 'no x-pkg-name'; exit; }
    if ($ver === '') $ver = substr(hash('sha256', $body), 0, 8);

    $dir = pkgs_dir() . '/' . $pkg;
    if (!is_dir($dir)) mkdir($dir, 0777, true);
    $store = $ver . '-' . $os . '.pdm';
    file_put_contents($dir . '/' . $store, $body);

    $meta = [
        'name' => $pkg, 'version' => $ver, 'file' => $store,
        'url'  => base_url() . '/packages/' . rawurlencode($pkg) . '/' . rawurlencode($store),
        'sha256' => hash('sha256', $body), 'os' => $os, 'description' => $desc,
    ];
    file_put_contents($dir . '/' . $ver . '-' . $os . '.json', json_encode($meta, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    // latest pointer + variants
    $latestPath = pkgs_dir() . '/' . $pkg . '.json';
    $last = [];
    if (is_file($latestPath)) { $last = json_decode(file_get_contents($latestPath), true) ?: []; }
    $variants = is_array($last['variants'] ?? null) ? $last['variants'] : [];
    $variants = array_values(array_filter($variants, fn($v) => !($v['version'] === $ver && $v['os'] === $os)));
    $variants[] = ['version' => $ver, 'os' => $os, 'file' => $store, 'url' => $meta['url'],
                   'sha256' => $meta['sha256'], 'description' => $desc];
    $latest = $meta + ['variants' => $variants];
    file_put_contents($latestPath, json_encode($latest, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    send_json(['ok' => true, 'file' => $store, 'version' => $ver, 'size' => strlen($body),
               'sha256' => $meta['sha256'], 'pkg' => $pkg, 'url' => $meta['url'], 'mirror' => base_url()]);
}

function serve_single_json($rel) {
    $f = pkgs_dir() . '/' . $rel;
    if (is_file($f)) send_file($f, 'application/json');
    http_response_code(404); echo 'not found'; exit;
}

// ------- router -------
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Headers: *');
if (($_SERVER['REQUEST_METHOD'] ?? '') === 'OPTIONS') { http_response_code(204); exit; }

$uri = parse_url($_SERVER['REQUEST_URI'] ?? '/', PHP_URL_PATH);
$uri = rawurldecode($uri);

if (preg_match('#^/upload$#', $uri)) {
    if (($_SERVER['REQUEST_METHOD'] ?? '') === 'POST') { handle_upload(); }
    else { http_response_code(405); echo 'method not allowed'; exit; }
}

if ($uri === '/' || $uri === '/index.html') send_file($ROOT . '/index.html', 'text/html; charset=utf-8');
if ($uri === '/packages.json' || $uri === '/api/packages.json') send_index();
if ($uri === '/mirror.ini' || $uri === '/mirror.conf') { header('Content-Type: text/plain'); echo mirror_ini(); exit; }

// /api/packages/<pkg>.json  or  /packages/<pkg>.json
if (preg_match('#^/(?:api/)?packages/([^/]+)\.json$#', $uri, $m)) serve_single_json($m[1] . '.json');
// /packages/<pkg>/<ver>-<os>.json|.pdm
if (preg_match('#^/(?:api/)?packages/([^/]+)/([^/]+)\.(json|pdm)$#', $uri, $m)) {
    $f = pkgs_dir() . '/' . $m[1] . '/' . $m[2] . '.' . $m[3];
    send_file($f, ($m[3] === 'json') ? 'application/json' : 'application/octet-stream');
}
if ($uri === '/api/health') { echo 'ok'; exit; }
http_response_code(404); echo 'not found';
