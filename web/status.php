<?php
/* Detailed status probe for the PMM mirror servers.
 * For each mirror we probe three endpoints (registry index / package download /
 * web dir listing) and report HTTP code, latency, server software and a
 * timestamp. Output is JSON for servers.php. No server IPs are exposed. */
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

$servers = [
    ['name' => '深圳', 'region' => 'CN · 深圳', 'host' => 'sz.pmm.parlz.com'],
    ['name' => '香港', 'region' => 'HK · 香港', 'host' => 'pmm.parlz.com'],
];

/* Probe one URL: returns [code, ms, server, ctype].
 * Prefers the PHP curl extension (reliable CURLINFO_TOTAL_TIME for ms, and works
 * where shell_exec/curl-binary are unavailable); falls back to shell curl. */
function probe_url(string $url): array {
    $code = 0; $ms = 0; $server = ''; $ctype = '';

    /* 1) PHP curl extension */
    if (function_exists('curl_init')) {
        $ch = curl_init($url);
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_NOBODY => true,
            CURLOPT_HEADER => true,
            CURLOPT_CONNECTTIMEOUT => 3,
            CURLOPT_TIMEOUT => 8,
            CURLOPT_FOLLOWLOCATION => true,
            CURLOPT_SSL_VERIFYPEER => false,
            CURLOPT_SSL_VERIFYHOST => false,
            CURLOPT_USERAGENT => 'pmm-status/1.0',
        ]);
        $resp = curl_exec($ch);
        $code = (int)curl_getinfo($ch, CURLINFO_HTTP_CODE);
        $ms   = (int)round(((float)curl_getinfo($ch, CURLINFO_TOTAL_TIME)) * 1000.0);
        $ctype = (string)curl_getinfo($ch, CURLINFO_CONTENT_TYPE);
        if (preg_match('/^Server:\s*(.+)$/mi', (string)$resp, $m)) $server = trim($m[1]);
        curl_close($ch);
        if ($code > 0) return [$code, $ms, $server, $ctype];
    }

    /* 2) shell curl (works where the binary exists; parses -D - headers + -w marker) */
    $cmd = 'curl -k -s -o /dev/null -D - --max-time 8 '
         . '-w "__PMM_PROBE__%{http_code} %{time_total}" ' . escapeshellarg($url);
    $out = (string)@shell_exec($cmd);
    if ($out !== '') {
        /* headers before the marker, "code ms" after it */
        $pos = strrpos($out, '__PMM_PROBE__');
        if ($pos !== false) {
            $meta = trim(substr($out, $pos + strlen('__PMM_PROBE__')));
            $parts = preg_split('/\s+/', $meta);
            if (isset($parts[0])) $code = (int)$parts[0];
            if (isset($parts[1])) $ms = (int)round(((float)$parts[1]) * 1000);
            $head = substr($out, 0, $pos);
            if (preg_match('/^Server:\s*(.+)$/mi', $head, $m)) $server = trim($m[1]);
            if (preg_match('/^Content-Type:\s*(.+)$/mi', $head, $m)) $ctype = trim($m[1]);
        }
    }
    return [$code, $ms, $server, $ctype];
}

function probe_check(string $url): array {
    [$code, $ms, $server, $ctype] = probe_url($url);
    return [
        'ok'     => ($code >= 200 && $code < 500 && $code !== 0),
        'code'   => $code,
        'ms'     => $ms,
        'server' => $server,
        'ctype'  => $ctype,
    ];
}

$out = [];
foreach ($servers as $s) {
    $h = $s['host'];
    $reg = probe_check("https://{$h}/mirror/packages/pmm.json");
    $dl  = probe_check("https://{$h}/mirror/packages/pmm/" . PMM_VERSION . "-linux-amd64.pdm");
    $web = probe_check("https://{$h}/mirror/");
    $avg = 0; $n = 0;
    foreach ([$reg, $dl, $web] as $c) if ($c['ok']) { $avg += $c['ms']; $n++; }
    $avg = $n ? (int)round($avg / $n) : 0;
    $out[] = [
        'name'    => $s['name'],
        'region'  => $s['region'],
        'host'    => $h,
        'online'  => $reg['ok'],
        'avg_ms'  => $avg,
        'checks'  => [
            ['item' => '注册表 (packages API)', 'url' => "https://{$h}/mirror/packages/pmm.json", 'result' => $reg],
            ['item' => '下载服务 (.pdm 分发)',  'url' => "https://{$h}/mirror/packages/pmm/" . PMM_VERSION . "-linux-amd64.pdm", 'result' => $dl],
            ['item' => 'Web 目录浏览',          'url' => "https://{$h}/mirror/",                  'result' => $web],
        ],
        'checked_at' => gmdate('Y-m-d\TH:i:s\Z'),
    ];
}
echo json_encode($out, JSON_UNESCAPED_UNICODE);
