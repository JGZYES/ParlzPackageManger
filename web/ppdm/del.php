<?php
/* web/ppdm/del.php — delete a package (or one version) that the current account
 * owns. Requires a valid bearer token.
 *   ?name=<pkg>[&version=<ver>]   ver optional: omit to remove the whole package.
 * Returns {"ok":true,...} on success. A non-owner or unknown package is rejected. */
define('PMM_SITE', 1);
require __DIR__ . '/lib.php';

[$tok, $email] = pmu_require_auth_token();

$name = trim((string)($_GET['name'] ?? ($_POST['name'] ?? '')));
$ver  = trim((string)($_GET['version'] ?? ($_POST['version'] ?? '')));
if (!preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]*$/', $name)) pmu_fail('invalid package name');
if ($ver !== '' && !preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]*$/', $ver)) pmu_fail('invalid version');

$regDir    = __DIR__ . '/../mirror/dists';
$filesRoot = __DIR__ . '/../mirror/files';
$pkgJson   = $regDir . '/' . $name . '.json';

if (!is_file($pkgJson)) pmu_fail("package '$name' not found", 404);
$m = json_decode(file_get_contents($pkgJson), true) ?: null;
if (($m['owner'] ?? '') !== $email) pmu_fail("you are not the owner of '$name'", 403);

$letter = strtolower(substr($name, 0, 1));
$pkgDir = $filesRoot . '/' . $letter . '/' . $name;

/* delete just one version */
if ($ver !== '') {
    $new = []; $removed = 0;
    foreach (($m['variants'] ?? []) as $v) {
        if (($v['version'] ?? '') === $ver) {
            if (is_file($pkgDir . '/' . ($v['file'] ?? ''))) @unlink($pkgDir . '/' . $v['file']);
            $removed++;
        } else {
            $new[] = $v;
        }
    }
    if (!$removed) pmu_fail("version '$ver' not found in '$name'", 404);
    $m['variants'] = $new;
    if (!empty($new)) {
        file_put_contents($pkgJson, json_encode($m, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES));
    } else {
        /* last version gone -> drop the whole package below */
        if (is_file($pkgJson)) @unlink($pkgJson);
    }
    @unlink($regDir . '/' . $name . '/' . $ver . '.json');
    if (empty($new)) {
        $vd = $regDir . '/' . $name;
        if (is_dir($vd)) { foreach (glob($vd . '/*') as $f) @unlink($f); @rmdir($vd); }
        $pk = $regDir . '/packages.json';
        $list = is_file($pk) ? (json_decode(file_get_contents($pk), true) ?: []) : [];
        $list = array_values(array_filter($list, fn($n) => $n !== $name));
        if (is_file($pk)) file_put_contents($pk, json_encode($list, JSON_PRETTY_PRINT));
    }
    pmu_ok(['name' => $name, 'version' => $ver, 'removed' => $removed]);
}

/* delete the whole package */
if (is_dir($pkgDir)) { foreach (glob($pkgDir . '/*') as $f) @unlink($f); @rmdir($pkgDir); }
if (is_file($pkgJson)) @unlink($pkgJson);
$vd = $regDir . '/' . $name;
if (is_dir($vd)) { foreach (glob($vd . '/*') as $f) @unlink($f); @rmdir($vd); }
$pk = $regDir . '/packages.json';
$list = is_file($pk) ? (json_decode(file_get_contents($pk), true) ?: []) : [];
$list = array_values(array_filter($list, fn($n) => $n !== $name));
if (is_file($pk)) file_put_contents($pk, json_encode($list, JSON_PRETTY_PRINT));

pmu_ok(['name' => $name, 'removed' => true]);
