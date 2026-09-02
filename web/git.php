<?php
/* git.php — GitHub-style browser for a LOCAL stored copy of the PMM repository.
 * Reads the git repo at $REPO_DIR (default web/repo/), a local clone stored on
 * the host. No online clone on view; if the repo isn't stored yet, show setup
 * instructions + a one-click clone/pull. */
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';

$REPO_URL = 'https://github.com/JGZYES/ParlzPackageManger.git';
/* Local repo clone lives in web/git (i.e. /www/sites/ParlzPackageManger/index/web/git
 * on the live host). PMM_SRC_DIR may override it. PHP needs write access there. */
$REPO_DIR = getenv('PMM_SRC_DIR') ?: (__DIR__ . '/git');

/* handle the one-click clone/pull early (no page output yet) */
if (isset($_GET['action']) && $_GET['action'] === 'init') {
    header('Content-Type: text/plain; charset=utf-8');
    if (!is_dir($REPO_DIR)) @mkdir($REPO_DIR, 0775, true);
    if (!is_dir($REPO_DIR . '/.git')) {
        $cmd = 'git clone --depth 1 ' . escapeshellarg($REPO_URL) . ' ' . escapeshellarg($REPO_DIR) . ' 2>&1';
        $out = (string)@shell_exec($cmd);
        if (is_dir($REPO_DIR . '/.git')) echo 'OK';
        else echo '克隆失败：' . substr($out, 0, 220) . "\n（请确保 PHP 对 " . $REPO_DIR . " 可写：chmod -R 775 " . $REPO_DIR . " 或其父目录；或设 PMM_SRC_DIR）";
    } else {
        $out = (string)@shell_exec('git -C ' . escapeshellarg($REPO_DIR) . ' pull --depth 1 2>&1');
        echo 'OK (pull: ' . substr(trim($out), 0, 80) . ')';
    }
    exit;
}

$repo_ok = is_dir($REPO_DIR . '/.git');

function sh(string $c): string { return trim((string)@shell_exec($c)); }
function repo_path(string $rel): string {
    global $REPO_DIR;
    $base = realpath($REPO_DIR);
    $full = realpath($REPO_DIR . '/' . $rel);
    if (!$base || !$full) return '';
    if (strpos(str_replace('\\', '/', $full), str_replace('\\', '/', $base)) !== 0) return '';
    return $full;
}
function esc(string $s): string { return htmlspecialchars($s, ENT_QUOTES, 'UTF-8'); }
function fmt_size(int $b): string {
    if ($b < 1024) return $b . ' B';
    if ($b < 1048576) return round($b / 1024, 1) . ' KB';
    return round($b / 1048576, 1) . ' MB';
}
function md(string $t): string {
    $t = htmlspecialchars($t, ENT_QUOTES, 'UTF-8');
    $t = preg_replace('/```([a-zA-Z0-9]*)\n(.*?)```/s', "<pre class=\"mdcode\"><code>\\2</code></pre>", $t);
    $t = preg_replace('/^### (.*)$/m', '<h3>$1</h3>', $t);
    $t = preg_replace('/^## (.*)$/m', '<h2>$1</h2>', $t);
    $t = preg_replace('/^# (.*)$/m', '<h1>$1</h1>', $t);
    $t = preg_replace('/^[-*] (.*)$/m', '<li>$1</li>', $t);
    $t = preg_replace('/^\s*---\s*$/m', '<hr>', $t);
    $t = preg_replace('/\*\*(.+?)\*\*/', '<strong>$1</strong>', $t);
    $t = preg_replace('/\[([^\]]+)\]\(([^)]+)\)/', '<a href="$2" target="_blank" rel="noopener">$1</a>', $t);
    $t = preg_replace('/`([^`]+)`/', '<code>$1</code>', $t);
    return $t;
}
function gh_api(string $url): array {
    $json = null;
    if (function_exists('curl_init')) {
        $ch = curl_init($url);
        curl_setopt_array($ch, [CURLOPT_RETURNTRANSFER => true, CURLOPT_TIMEOUT => 15,
            CURLOPT_FOLLOWLOCATION => true, CURLOPT_SSL_VERIFYPEER => false,
            CURLOPT_SSL_VERIFYHOST => false, CURLOPT_USERAGENT => 'pmm-site']);
        $r = curl_exec($ch); curl_close($ch);
        if (is_string($r) && $r !== '') $json = $r;
    }
    if ($json === null) $json = (string)@shell_exec('curl -s --max-time 15 ' . escapeshellarg($url));
    $d = json_decode((string)$json, true);
    return is_array($d) ? $d : [];
}
/* Prefer a local copy of a release asset on this server; fall back to GitHub. */
function local_asset(string $name): ?string {
    foreach ([__DIR__ . '/downloads/' . $name, __DIR__ . '/mirror/packages/pmm/' . $name] as $p) {
        if (@is_file($p)) {
            $base = realpath(__DIR__);
            return '/' . str_replace('\\', '/', substr(str_replace('\\', '/', realpath($p)), strlen($base) + 1));
        }
    }
    return null;
}
function render_releases(): void {
    $rels = gh_api('https://api.github.com/repos/JGZYES/ParlzPackageManger/releases?per_page=30');
    if (!$rels) { echo '<div class="tips">无法获取 Releases（网络或 API 受限）。</div>'; return; }
    echo '<div class="rel-list">';
    foreach ($rels as $r):
        $tag = $r['tag_name'] ?? '?'; $name = $r['name'] ?? $tag;
        $date = $r['published_at'] ?? ($r['created_at'] ?? '');
        $date = substr(str_replace('T', ' ', (string)$date), 0, 10);
        $prere = !empty($r['prerelease']);
        $assets = $r['assets'] ?? [];
?>
      <div class="rel-card">
        <div class="rel-head">
          <div>
            <span class="rel-version"><?php echo esc($name); ?></span>
            <?php if ($prere): ?><span class="svc-badge">预发布</span><?php endif; ?>
            <span class="rel-tag"><?php echo esc($tag); ?></span>
          </div>
          <span class="rel-date"><?php echo esc($date); ?></span>
        </div>
        <?php if ((string)($r['body'] ?? '') !== ''): ?>
        <div class="mdbody"><?php echo md((string)$r['body']); ?></div>
        <?php endif; ?>
        <?php if ($assets): ?>
        <div class="rel-assets">
          <?php foreach ($assets as $a): ?>
            <?php $local = local_asset((string)($a['name'] ?? '')); $href = $local ?: ($a['browser_download_url'] ?? '#'); ?>
            <a class="rel-asset<?php echo $local ? ' local' : ''; ?>" href="<?php echo esc($href); ?>" target="_blank" rel="noopener">
              <?php echo esc($a['name'] ?? 'asset'); ?> <span class="rel-size"><?php echo isset($a['size']) ? fmt_size((int)$a['size']) : ''; ?></span>
            </a>
          <?php endforeach; ?>
        </div>
        <?php endif; ?>
      </div>
<?php endforeach; echo '</div>';
}

$view = $_GET['view'] ?? '';
$branch = $repo_ok ? sh("git -C " . escapeshellarg($REPO_DIR) . " rev-parse --abbrev-ref HEAD 2>/dev/null") : '';
$last   = $repo_ok ? sh('git -C ' . escapeshellarg($REPO_DIR) . " log -1 --format=\"%H|%s|%an|%ad\" --date=short 2>/dev/null") : '';
[$lhash, $lsubj, $luser, $ldate] = array_pad(explode('|', $last, 4), 4, '');
$lhash7 = substr($lhash, 0, 7);

pmm_header('source', '源码浏览');
?>

<header class="page-hero"><div class="wrap">
  <div class="page-kicker">SOURCE</div>
  <h1>源码 <span class="accent">浏览</span></h1>
  <div class="src-clone-note">
    <?php echo $repo_ok
      ? '本地仓库 <code>JGZYES/ParlzPackageManager</code> · 分支 <code>' . esc($branch) . '</code> · ' . esc($REPO_DIR)
      : '提示：本地依赖仓库尚未初始化，请先 <code>git clone</code> 或点击下方“初始化/更新”。'; ?>
  </div>
</div></header>

<section class="section" style="padding-top:18px">
  <div class="wrap">

<?php if (!$repo_ok): ?>
    <div class="gh-card">
      <div class="gh-repo">初始化本地源码仓库</div>
      <p class="src-clone-note">页面将 PMM 仓库副本存入 <code><?php echo esc($REPO_DIR); ?></code>。若 PHP 无写权限（web 根归 root），先
      <code>chmod -R 775 <?php echo esc($REPO_DIR); ?></code> 或其父目录；或用环境变量 <code>PMM_SRC_DIR</code> 指定持久目录。初始化命令：</p>
      <pre class="gh-code"><code>git clone --depth 1 <?php echo esc($REPO_URL); ?> <?php echo esc($REPO_DIR); ?></code></pre>
      <div style="margin-top:12px">
        <button class="btn btn-sm" id="git-now" type="button">在线初始化 / 更新</button>
        <span id="git-msg" class="src-clone-note" style="margin-left:10px"></span>
      </div>
    </div>
<?php else:
  $rel = $_GET['path'] ?? '';
  $rel = str_replace(['../', '..\\', "\0"], '', $rel);
  $rel = ltrim($rel, '/');
  $abs = repo_path($rel);
  if (!$abs) { $abs = realpath($REPO_DIR); $rel = ''; }
  $is_file = is_file($abs);
  $parts = $rel === '' ? [] : explode('/', $rel);
?>
    <div class="gh-card">
      <div class="gh-top">
        <a class="gh-repo" href="<?php echo $REPO_URL; ?>" target="_blank" rel="noopener">JGZYES / ParlzPackageManager</a>
        <div class="gh-actions">
          <a class="btn btn-sm" href="git.php">Code</a>
          <a class="btn btn-sm" href="git.php?view=releases">Releases</a>
          <span class="gh-branch">▸ <?php echo esc($branch ?: 'main'); ?></span>
        </div>
      </div>
      <div class="gh-commit">
        <span class="gh-hash" title="<?php echo esc($lhash); ?>"><?php echo esc($lhash7); ?></span>
        <span class="gh-subj"><?php echo esc($lsubj); ?></span>
        <span class="gh-meta"><?php echo esc($luser); ?> · <?php echo esc($ldate); ?></span>
      </div>
    </div>

    <?php if ($view === 'releases'): ?>
      <h2 class="section-title" style="text-align:left;font-size:22px;margin:6px 0 20px">Releases</h2>
      <?php render_releases(); ?>
    <?php else: ?>
    <div class="gh-path">
      <a href="git.php"><code>source</code></a>
      <?php $acc=''; foreach ($parts as $p) { $acc = $acc==='' ? $p : $acc.'/'.$p;
          echo ' / <a href="git.php?path=' . urlencode($acc) . '">' . esc($p) . '</a>'; } ?>
    </div>

<?php if ($is_file): ?>
    <div class="gh-filehead"><span class="gh-filelink"><?php echo esc(end($parts)); ?></span>
      <span class="gh-meta"><?php echo fmt_size((int)filesize($abs)); ?> bytes</span></div>
    <pre class="gh-code"><code><?php echo esc(substr((string)@file_get_contents($abs), 0, 400000)); ?></code></pre>
<?php else:
  $dirs = []; $files = [];
      foreach (scandir($abs) as $e) {
          if ($e === '.' || $e === '..' || $e === '.git') continue;
          $p = $abs . '/' . $e;
          if (is_dir($p)) $dirs[] = $e; else $files[] = $e;
      }
  sort($dirs); sort($files);
  $readme = '';
  foreach (['README.md', 'README.MD', 'readme.md'] as $r) if (is_file($abs . '/' . $r)) { $readme = $abs . '/' . $r; break; }
?>
    <table class="gh-list">
      <thead><tr><th>名称</th><th>提交</th><th>时间</th><th>大小</th></tr></thead>
      <tbody>
      <?php
      $rows = array_merge(array_map(fn($d) => [true, $d], $dirs), array_map(fn($f) => [false, $f], $files));
      foreach ($rows as [$isD, $name]):
          $sub = $rel === '' ? $name : "$rel/$name";
          $href = 'git.php?path=' . urlencode($sub);
          $commit = sh('git -C ' . escapeshellarg($REPO_DIR) . " log -1 --format=\"%h|%an|%ad\" --date=short -- " . escapeshellarg($sub) . ' 2>/dev/null');
          [$ch, $ca, $cd] = array_pad(explode('|', $commit, 3), 3, '');
      ?>
        <tr>
          <td class="gh-name"><a class="<?php echo $isD ? 'gh-dir' : 'gh-file'; ?>" href="<?php echo $href; ?>">
            <span class="gh-ico"><?php echo $isD ? '&#128193;' : '&#128196;'; ?></span> <?php echo esc($name); ?></a></td>
          <td class="gh-c"><code><?php echo esc($ch); ?></code> <?php echo esc($ca); ?></td>
          <td class="gh-c"><?php echo esc($cd); ?></td>
          <td class="gh-c"><?php echo $isD ? '—' : fmt_size((int)@filesize($abs . '/' . $name)); ?></td>
        </tr>
      <?php endforeach; ?>
      </tbody>
    </table>
    <?php if ($readme): ?>
    <div class="gh-readme"><div class="gh-readme-t">README</div><div class="mdbody"><?php echo md((string)@file_get_contents($readme)); ?></div></div>
    <?php endif; ?>
<?php endif; endif; endif; ?>

  </div>
</section>

<script>
(function(){
  var btn = document.getElementById("git-now");
  if (btn) btn.addEventListener("click", function(){
    var msg = document.getElementById("git-msg");
    msg.textContent = "正在初始化…";
    fetch("git.php?action=init", { cache: "no-store" })
      .then(function(r){ return r.text(); })
      .then(function(t){ msg.textContent = t; if (t.indexOf("OK") === 0) setTimeout(function(){ location.reload(); }, 1200); })
      .catch(function(){ msg.textContent = "失败"; });
  });
})();
</script>
<?php pmm_footer(); ?>