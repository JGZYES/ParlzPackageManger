<?php
/* web/ppdm/list.php — list packages owned by the current account.
 * Requires a valid bearer token. Returns:
 *   {"ok":true,"list":"name@ver  os/arch  description  url\n..."}
 * The list field uses real newlines (the tiny C client reads it via substring
 * search, so we deliberately do NOT json_encode the newlines). */
define('PMM_SITE', 1);
require __DIR__ . '/lib.php';

[$tok, $email] = pmu_require_auth_token();

$regDir = __DIR__ . '/../mirror/dists';
$out = [];
foreach (glob($regDir . '/*.json') as $f) {
    if (basename($f) === 'packages.json') continue;
    $m = json_decode(file_get_contents($f), true) ?: [];
    if (($m['owner'] ?? '') !== $email) continue;
    foreach (($m['variants'] ?? []) as $v) {
        $nm   = $v['name'] ?? ($m['name'] ?? '');
        $ver  = $v['version'] ?? ($m['version'] ?? '');
        $os   = $v['os'] ?? 'linux';
        $arch = $v['arch'] ?? 'amd64';
        $desc = $v['description'] ?? '';
        $url  = $v['url'] ?? '';
        $out[] = sprintf("%s@%s  %s/%s  %s  %s", $nm, $ver, $os, $arch, $desc, $url);
    }
}
$text = implode("\n", $out);
echo '{"ok":true,"list":"' . $text . '"}';
