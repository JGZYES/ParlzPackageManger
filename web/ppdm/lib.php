<?php
/* web/pmu/lib.php — shared helpers for the PMU package-upload service.
 * Multi-value state lives in web/pmu/data/*.json (gitignored). */
define('PMU_DATA', __DIR__ . '/data');
if (!is_dir(PMU_DATA)) @mkdir(PMU_DATA, 0777, true);

function pmu_json($arr) {
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode($arr, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}
function pmu_ok($extra = []) { pmu_json(array_merge(['ok' => true], $extra)); }
function pmu_fail($msg, $code = 400) { http_response_code($code); pmu_json(['ok' => false, 'error' => $msg]); }

function pmu_read_json() {
    $raw = file_get_contents('php://input');
    $d = json_decode($raw ?: 'null', true);
    return is_array($d) ? $d : [];
}

function pmu_load($file) {
    $p = PMU_DATA . '/' . $file;
    return is_file($p) ? (json_decode(file_get_contents($p), true) ?: []) : [];
}
function pmu_save($file, $arr) {
    file_put_contents(PMU_DATA . '/' . $file,
        json_encode($arr, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES));
}

function pmu_token() { return bin2hex(random_bytes(24)); }

/* Resolve the caller identity from a bearer token (Authorization: Bearer <tok>,
 * or ?token=, or POST/token= for clients that can't set headers). */
function pmu_require_auth_token() {
    $hdr = $_SERVER['HTTP_AUTHORIZATION'] ?? '';
    $tok = null;
    if (preg_match('/Bearer\s+(.+)/i', $hdr, $m)) $tok = trim($m[1]);
    if (!$tok) $tok = $_GET['token'] ?? ($_POST['token'] ?? null);
    if (!$tok) pmu_fail('unauthorized', 401);
    $s = pmu_load('sessions.json');
    $email = $s[$tok] ?? null;
    if (!$email) pmu_fail('invalid token', 401);
    return [$tok, $email];
}
