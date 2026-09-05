<?php
/* web/pmu/login.php — verify credentials, issue a session token. */
define('PMM_SITE', 1);
require __DIR__ . '/lib.php';

$d = pmu_read_json();
$email = trim((string)($d['email'] ?? ''));
$pass  = (string)($d['password'] ?? '');
$users = pmu_load('users.json');
if (!isset($users[$email]) || !password_verify($pass, $users[$email]['hash'])) pmu_fail('invalid credentials', 401);

$tok = pmu_token();
$s = pmu_load('sessions.json');
$s[$tok] = $email;
pmu_save('sessions.json', $s);
pmu_ok(['token' => $tok, 'email' => $email]);
