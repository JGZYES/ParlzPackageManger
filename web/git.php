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
/* Prefer a local copy of a release asset (web/releases/<tag>/, web/downloads/,
 * web/mirror/, web root) on this server; fall back to GitHub. */
function local_asset(string $name, string $tag = ''): ?string {
    $cands = [];
    if ($tag !== '') $cands[] = __DIR__ . '/releases/' . $tag . '/' . $name;
    $cands = array_merge($cands, [__DIR__ . '/downloads/' . $name, __DIR__ . '/mirror/packages/pmm/' . $name, __DIR__ . '/' . $name]);
    $base = realpath(__DIR__);
    foreach ($cands as $p) {
        if (@is_file($p)) {
            $rp = realpath($p);
            if (strpos($rp, $base) !== 0) continue;
            return '/' . str_replace('\\', '/', substr(str_replace('\\', '/', $rp), strlen($base) + 1));
        }
    }
    return null;
}
function render_releases(): void {
    $rels = gh_api('https://api.github.com/repos/JGZYES/ParlzPackageManger/releases?per_page=30');
    if (!$rels) { echo '<div class="tips">无法获取 Releases（网络或 API 受限）。</div>'; return; }
    /* left: tag list (like GitHub's folded releases) */
    echo '<div class="rel-layout"><aside class="rel-tags"><div class="rel-tags-h">Releases</div><ul>';
    $i = 0;
    foreach ($rels as $r) {
        $t = $r['tag_name'] ?? '?';
        echo '<li' . ($i === 0 ? ' class="on"' : '') . '><a href="#rel-' . esc($t) . '">' . esc($t) . '</a></li>';
        $i++;
    }
    echo '</ul></aside>';
    /* right: release entries */
    echo '<div class="rel-main">';
    $i = 0;
    foreach ($rels as $r):
        $tag = $r['tag_name'] ?? '?'; $name = $r['name'] ?? $tag;
        $date = $r['published_at'] ?? ($r['created_at'] ?? '');
        $date = substr(str_replace('T', ' ', (string)$date), 0, 10);
        $prere = !empty($r['prerelease']);
        $assets = $r['assets'] ?? [];
        $body = (string)($r['body'] ?? '');
?>
    <div class="rel-entry" id="rel-<?php echo esc($tag); ?>"<?php echo $i === 0 ? ' data-latest="1"' : ''; ?>>
      <div class="rel-card">
        <div class="rel-head">
          <h2 class="rel-version"><span class="rel-ico">&#10225;</span> <?php echo esc($name); ?></h2>
          <div class="rel-badges">
            <?php if ($i === 0): ?><span class="rel-latest">Latest</span><?php endif; ?>
            <?php if ($prere): ?><span class="rel-latest prere">Pre-release</span><?php endif; ?>
            <span class="rel-date"><?php echo esc($date); ?></span>
          </div>
        </div>
        <?php if ($body !== ''): ?>
        <div class="rel-body mdbody"><?php echo md($body); ?></div>
        <?php endif; ?>
        <p class="rel-tagline">标签 <code><?php echo esc($tag); ?></code></p>
      </div>
      <?php if ($assets): ?>
      <div class="rel-assets-card">
        <div class="rel-assets-h">Assets · 下载</div>
        <div class="rel-assets-list">
          <?php foreach ($assets as $a):
              $local = local_asset((string)($a['name'] ?? ''), $tag);
              $href = $local ?: ($a['browser_download_url'] ?? '#');
              $sz = isset($a['size']) ? fmt_size((int)$a['size']) : '';
          ?>
          <div class="rel-asset-row">
            <a class="rel-asset<?php echo $local ? ' local' : ''; ?>"
               href="<?php echo esc($href); ?>" title="<?php echo esc($href); ?>"
               <?php echo $local ? ' download' : ' target="_blank" rel="noopener"'; ?>>
              <span class="rel-a-ico">&#128196;</span> <?php echo esc($a['name'] ?? 'asset'); ?>
            </a>
            <span class="rel-size"><?php echo esc($sz); ?></span>
            <a class="rel-dl" href="<?php echo esc($href); ?>" <?php echo $local ? ' download' : ' target="_blank" rel="noopener"'; ?>>下载</a>
          </div>
          <?php endforeach; ?>
        </div>
      </div>
      <?php endif; ?>
    </div>
<?php $i++; endforeach;
    echo '</div></div>';
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
      <style>
        .rel-layout{display:grid;grid-template-columns:220px 1fr;gap:26px;align-items:start}
        @media(max-width:840px){.rel-layout{grid-template-columns:1fr}}
        .rel-tags{position:sticky;top:70px;border-right:1px solid var(--line);padding-right:12px}
        .rel-tags-h{font-size:11px;letter-spacing:1.5px;text-transform:uppercase;color:var(--dim);padding:8px 4px;font-weight:700}
        .rel-tags ul{list-style:none;margin:0;padding:0;display:grid;gap:2px}
        .rel-tags li a{display:block;padding:6px 10px;border-radius:6px;font-family:ui-monospace,monospace;font-size:12.5px;color:var(--muted);text-decoration:none}
        .rel-tags li a:hover{background:var(--panel2);color:#fff}
        .rel-tags li.on a{background:var(--panel);color:#fff;font-weight:600}
        .rel-main{display:grid;gap:24px;min-width:0}
        .rel-entry{display:grid;gap:14px}
        .rel-card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:20px 22px}
        .rel-assets-card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:16px 22px}
        .rel-head{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-bottom:10px}
        .rel-version{font-weight:800;font-size:19px;color:#fff;margin:0;display:flex;align-items:center;gap:9px;letter-spacing:-.3px}
        .rel-ico{color:var(--dim);font-size:16px}
        .rel-badges{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
        .rel-latest{font-size:10.5px;font-weight:700;letter-spacing:.6px;text-transform:uppercase;color:#111;background:#fff;border-radius:999px;padding:2px 9px}
        .rel-latest.prere{color:#fff;background:transparent;border:1px solid var(--line);font-weight:600}
        .rel-date{font-size:12.5px;color:var(--dim);font-family:ui-monospace,monospace}
        .rel-body{border-top:1px solid var(--line);padding-top:14px;margin-top:4px}
        .rel-tagline{font-size:12px;color:var(--dim);margin-top:12px}
        .rel-assets-card .rel-assets-h{font-size:11px;letter-spacing:1.3px;text-transform:uppercase;color:var(--dim);margin-bottom:12px;font-weight:700}
        .rel-assets-list{display:grid;gap:8px}
        .rel-asset-row{display:flex;align-items:center;gap:12px;padding:10px 12px;border:1px solid var(--line);border-radius:8px;background:#0c1017}
        .rel-asset{display:inline-flex;align-items:center;gap:7px;color:var(--text);font-family:ui-monospace,monospace;font-size:12.5px;text-decoration:none;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
        .rel-asset:hover{color:#fff;text-decoration:underline}
        .rel-asset .rel-a-ico{color:var(--dim)}
        .rel-asset.local{color:#fff}
        .rel-size{font-size:11.5px;color:var(--dim);font-family:ui-monospace,monospace;white-space:nowrap}
        .rel-dl{margin-left:auto;font-size:12px;color:var(--muted);border:1px solid var(--line);border-radius:6px;padding:3px 10px;text-decoration:none;white-space:nowrap}
        .rel-dl:hover{color:#fff;border-color:#fff}
      </style>
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