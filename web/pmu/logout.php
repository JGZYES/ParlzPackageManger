<?php
/* web/pmu/logout.php — revoke the caller's session token. */
define('PMM_SITE', 1);
require __DIR__ . '/lib.php';

[$tok, $email] = pmu_require_auth_token();
$s = pmu_load('sessions.json');
unset($s[$tok]);
pmu_save('sessions.json', $s);
pmu_ok(['logged_out_email' => $email]);
