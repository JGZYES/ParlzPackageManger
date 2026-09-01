<?php
/* Status probe for PMM mirror servers.
 * Server-side GET (curl -k, since we hit IPs/hosts that may not match the
 * browser's TLS expectations) -> returns JSON for the homepage status panel. */
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

$servers = [
    ['name'=>'深圳', 'ip'=>'120.24.78.176', 'url'=>'https://sz.pmm.parlz.com/mirror/packages/pmm.json'],
    ['name'=>'香港', 'ip'=>'38.76.190.153', 'url'=>'https://pmm.parlz.com/mirror/packages/pmm.json'],
];

function probe($url) {
    $t = microtime(true);
    /* 1) PHP curl extension (preferred) */
    if (function_exists('curl_init')) {
        $ch = curl_init($url);
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_NOBODY => true,
            CURLOPT_CONNECTTIMEOUT => 3,
            CURLOPT_TIMEOUT => 6,
            CURLOPT_SSL_VERIFYPEER => false,
            CURLOPT_SSL_VERIFYHOST => false,
            CURLOPT_FOLLOWLOCATION => true,
            CURLOPT_USERAGENT => 'pmm-status/1.0',
        ]);
        curl_exec($ch);
        $code  = (int)curl_getinfo($ch, CURLINFO_HTTP_CODE);
        $errno = curl_errno($ch);
        curl_close($ch);
        if ($errno === 0 && $code > 0)
            return ['online'=>$code < 500, 'code'=>$code, 'ms'=>(int)round((microtime(true)-$t)*1000)];
    }
    /* 2) shell curl (pmm depends on the curl binary, so it is present) */
    if (function_exists('shell_exec') && !in_array('shell_exec', array_map('trim', explode(',', ini_get('disable_functions'))))) {
        $cmd = 'curl -k -s -o /dev/null -w "%{http_code}" --connect-timeout 3 --max-time 6 ' . escapeshellarg($url);
        $out = trim((string)@shell_exec($cmd));
        $code = (int)$out;
        if ($code > 0)
            return ['online'=>$code < 500, 'code'=>$code, 'ms'=>(int)round((microtime(true)-$t)*1000)];
    }
    /* 3) stream context fallback */
    $ctx = stream_context_create([
        'http' => ['method'=>'HEAD','timeout'=>6,'follow_location'=>1,'ignore_errors'=>true],
        'ssl'  => ['verify_peer'=>false,'verify_peer_name'=>false],
    ]);
    $fp = @fopen($url, 'r', false, $ctx);
    $ms = (int)round((microtime(true) - $t) * 1000);
    if ($fp) { fclose($fp); return ['online'=>true,'code'=>200,'ms'=>$ms]; }
    return ['online'=>false,'code'=>0,'ms'=>$ms];
}

$out = [];
foreach ($servers as $s) {
    $st = probe($s['url']);
    $out[] = [
        'name'   => $s['name'],
        'ip'     => $s['ip'],
        'online' => $st['online'],
        'code'   => $st['code'],
        'ms'     => $st['ms'],
    ];
}
echo json_encode($out, JSON_UNESCAPED_UNICODE);
