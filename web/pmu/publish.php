<?php
/* web/pmu/publish.php — upload a completed .pdm (raw body) and publish it to the
 * mirror registry: writes/updates <pkg>.json, stores the .pdm under
 * web/mirror/packages/<pkg>/, and appends the name to packages.json.
 * Duplicate package names are rejected (409). Requires a valid bearer token. */
define('PMM_SITE', 1);
require __DIR__ . '/lib.php';

[$tok, $email] = pmu_require_auth_token();

$name = trim((string)($_GET['name'] ?? ''));
$ver  = trim((string)($_GET['version'] ?? ''));
$arch = trim((string)($_GET['arch'] ?? 'amd64'));
$os   = trim((string)($_GET['os'] ?? 'linux'));
$desc = trim((string)($_GET['description'] ?? $name));

if (!preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]*$/', $name)) pmu_fail('invalid package name');
if ($ver === '') pmu_fail('missing version');
if ($arch === '') $arch = 'amd64';
if ($os !== 'windows' && $os !== 'linux' && $os !== 'macos') pmu_fail('invalid os');

$body = file_get_contents('php://input');
if (strlen($body) === 0) pmu_fail('empty upload body');
if (strlen($body) > 100 * 1024 * 1024) pmu_fail('package too large (>100 MB)');

$regDir = __DIR__ . '/../mirror/dists';
$filesRoot = __DIR__ . '/../mirror/files';
$pkgJson = $regDir . '/' . $name . '.json';
$letter = strtolower(substr($name, 0, 1));

/* duplicate-name reject: the package name is already published */
if (is_file($pkgJson)) pmu_fail("package name '$name' already exists", 409);

$sha = hash('sha256', $body);
$file = $ver . '-' . $os . '-' . $arch . '.pdm';
$pkgDir = $filesRoot . '/' . $letter . '/' . $name;
if (!is_dir($pkgDir)) @mkdir($pkgDir, 0777, true);
file_put_contents($pkgDir . '/' . $file, $body);

$url = 'https://pmm.parlz.com/mirror/files/' . $letter . '/' . $name . '/' . $file;
$meta = [
    'name'        => $name,
    'version'     => $ver,
    'os'          => $os,
    'arch'        => $arch,
    'description' => $desc,
    'variants'    => [[
        'name'        => $name,
        'version'     => $ver,
        'os'          => $os,
        'arch'        => $arch,
        'file'        => $file,
        'url'         => $url,
        'sha256'      => $sha,
        'description' => $desc,
    ]],
];
file_put_contents($pkgJson, json_encode($meta, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES));

/* per-version metadata: dists/<name>/<version>.json */
$vdir = $regDir . '/' . $name;
if (!is_dir($vdir)) @mkdir($vdir, 0777, true);
file_put_contents($vdir . '/' . $ver . '.json',
    json_encode(['name' => $name, 'version' => $ver, 'variants' => $meta['variants']],
                JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES));

/* append the name to the aggregate index if new */
$pk = $regDir . '/packages.json';
$list = is_file($pk) ? (json_decode(file_get_contents($pk), true) ?: []) : [];
if (!in_array($name, $list, true)) { $list[] = $name; file_put_contents($pk, json_encode($list, JSON_PRETTY_PRINT)); }

pmu_ok(['name' => $name, 'version' => $ver, 'file' => $file, 'url' => $url, 'sha256' => $sha]);
