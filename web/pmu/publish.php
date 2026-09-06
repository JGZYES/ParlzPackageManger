<?php
/* web/pmu/publish.php — upload a completed .pdm (raw body) and publish it to the
 * mirror registry: writes/updates dists/<pkg>.json, stores the .pdm under
 * web/mirror/files/<first-letter>/<pkg>/, and appends the name to packages.json.
 *
 * Ownership rules:
 *  - a package that doesn't exist yet is created, and the uploader becomes owner;
 *  - the same owner may publish a NEW version of the same package (the version
 *    must not already exist);
 *  - a different creator is rejected (409).
 * Requires a valid bearer token. */
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

$regDir    = __DIR__ . '/../mirror/dists';
$filesRoot = __DIR__ . '/../mirror/files';
$pkgJson   = $regDir . '/' . $name . '.json';
$letter    = strtolower(substr($name, 0, 1));

$sha  = hash('sha256', $body);
$file = $ver . '-' . $os . '-' . $arch . '.pdm';
$url  = 'https://pmm.parlz.com/mirror/files/' . $letter . '/' . $name . '/' . $file;
$variant = [
    'name' => $name, 'version' => $ver, 'os' => $os, 'arch' => $arch,
    'file' => $file, 'url' => $url, 'sha256' => $sha, 'description' => $desc,
];

/* existing package? */
$meta = null;
if (is_file($pkgJson)) $meta = json_decode(file_get_contents($pkgJson), true) ?: null;

if ($meta) {
    /* ownership: only the original creator may add versions */
    $own = $meta['owner'] ?? '';
    if ($own !== $email) pmu_fail("package '$name' already exists (owner: $own)", 409);
    /* the version must be new */
    foreach (($meta['variants'] ?? []) as $v)
        if (($v['version'] ?? '') === $ver) pmu_fail("version '$ver' already published for $name", 409);
    /* append the new variant (mirror keeps all versions) */
    $meta['version'] = $ver;
    $meta['variants'][] = $variant;
} else {
    $meta = [
        'name' => $name, 'version' => $ver, 'os' => $os, 'arch' => $arch,
        'description' => $desc, 'owner' => $email, 'variants' => [$variant],
    ];
}

/* write the payload */
$pkgDir = $filesRoot . '/' . $letter . '/' . $name;
if (!is_dir($pkgDir)) @mkdir($pkgDir, 0777, true);
file_put_contents($pkgDir . '/' . $file, $body);

/* write the registry + per-version metadata */
file_put_contents($pkgJson, json_encode($meta, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES));
$vdir = $regDir . '/' . $name;
if (!is_dir($vdir)) @mkdir($vdir, 0777, true);
file_put_contents($vdir . '/' . $ver . '.json',
    json_encode(['name' => $name, 'version' => $ver, 'owner' => $email, 'variants' => [$variant]],
                JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES));

/* append the name to the aggregate index if new */
$pk = $regDir . '/packages.json';
$list = is_file($pk) ? (json_decode(file_get_contents($pk), true) ?: []) : [];
if (!in_array($name, $list, true)) { $list[] = $name; file_put_contents($pk, json_encode($list, JSON_PRETTY_PRINT)); }

pmu_ok(['name' => $name, 'version' => $ver, 'file' => $file, 'url' => $url, 'sha256' => $sha]);
