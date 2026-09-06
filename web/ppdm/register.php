<?php
/* web/pmu/register.php — create an account. Human verification is done client-side:
 * the pmu client asks a local arithmetic question before calling here. */
define('PMM_SITE', 1);
require __DIR__ . '/lib.php';

$d = pmu_read_json();
$email = trim((string)($d['email'] ?? ''));
$pass  = (string)($d['password'] ?? '');
if (filter_var($email, FILTER_VALIDATE_EMAIL) === false) pmu_fail('invalid email');
if (strlen($pass) < 6) pmu_fail('password too short (>= 6 chars)');

$users = pmu_load('users.json');
if (isset($users[$email])) pmu_fail('email already registered', 409);

$users[$email] = ['email' => $email, 'hash' => password_hash($pass, PASSWORD_DEFAULT), 'created' => time()];
pmu_save('users.json', $users);
pmu_ok(['email' => $email]);
