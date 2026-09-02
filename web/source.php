<?php
/* GitHub-style source browser for the PMM repository.
 * Clones the repo into a cache dir (web/.src) on first view, then serves the
 * git tree: file browser, code view, README, commit info. Dark grayscale UI. */
define('PMM_SITE', 1);
require __DIR__ . '/_common.php';

$REPO   = 'https://github.com/JGZYES/ParlzPackageManger.git';
$ROOT   = __DIR__ . '/.src';            /* cache clone (gitignored) */

/* --- ensure the clone exists (best-effort; degrade gracefully) --- */
$git_ok = false;
if (is_dir($ROOT . '/.git')) {
    $git_ok = true;
} else {
    @mkdir($ROOT, 0755, true);
    $cmd = 'git clone --depth 1 ' . escapeshellarg($REPO) . ' ' . escapeshellarg($ROOT) . ' 2>&1';
    @shell_exec($cmd);
    if (is_dir($ROOT . '/.git')) $git_ok = true;
}

function sh(string $c): string { return trim((string)@shell_exec($c)); }
function git_path(string $rel): string {
    global $ROOT;
    $full = realpath($ROOT . '/' . $rel);
    if (!$full) return '';
    if (strpos(str_replace('\\', '/', $full), str_replace('\\', '/', realpath($ROOT))) !== 0) return '';
    return $full;
}
function esc(string $s): string { return htmlspecialchars($s, ENT_QUOTES, 'UTF-8'); }
function fmt_size(int $b): string {
    if ($b < 1024) return $b . ' B';
    if ($b < 1048576) return round($b / 1024, 1) . ' KB';
    return round($b / 1048576, 1) . ' MB';
}
/* Lightweight markdown -> HTML (code fences, headings, lists, links, bold, hr). */
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

/* --- git meta --- */
$branch = $git_ok ? sh("git -C " . escapeshellarg($ROOT) . " rev-parse --abbrev-ref HEAD 2>/dev/null") : '';
$last   = $git_ok ? sh('git -C ' . escapeshellarg($ROOT) . ' log -1 --format=%H|%s|%an|%ad --date=short 2>/dev/null') : '';
[$lhash, $lsubj, $luser, $ldate] = array_pad(explode('|', $last, 4), 4, '');
$lhash7 = substr($lhash, 0, 7);

pmm_header('source', '源码浏览');
?>

<header class="page-hero"><div class="wrap">
  <div class="page-kicker">SOURCE</div>
  <h1>源码 <span class="accent">浏览</span></h1>
  <div class="src-clone-note"><?php echo $git_ok ?
        'Git 仓库 <code>JGZYES/ParlzPackageManager</code> · 分支 <code>' . esc($branch) . '</code>' :
        '提示：正在初始化 Git 仓库（首次访问会 <code>git clone</code>），若失败请检查服务器 git/网络。'; ?></div>
</div></header>

<section class="section" style="padding-top:18px">
  <div class="wrap">

    <div class="gh-card">
      <div class="gh-top">
        <a class="gh-repo" href="<?php echo $REPO; ?>" target="_blank" rel="noopener">JGZYES / ParlzPackageManager</a>
        <span class="gh-branch">▸ <?php echo esc($branch ?: 'main'); ?></span>
      </div>
      <?php if ($git_ok): ?>
      <div class="gh-commit">
        <span class="gh-hash" title="<?php echo esc($lhash); ?>"><?php echo esc($lhash7); ?></span>
        <span class="gh-subj"><?php echo esc($lsubj); ?></span>
        <span class="gh-meta"><?php echo esc($luser); ?> · <?php echo esc($ldate); ?></span>
      </div>
      <?php endif; ?>
    </div>

<?php
if (!$git_ok) {
    echo '<div class="tips">无法克隆仓库（服务端缺少 git 或网络受限）。可手动把源码放到 <code>web/.src/</code> 目录。</div></div></section>';
    pmm_footer(); exit;
}

$rel = $_GET['path'] ?? '';
$rel = str_replace(['../', '..\\', "\0"], '', $rel);
$rel = ltrim($rel, '/');
$abs = git_path($rel);
if (!$abs) { $abs = realpath($ROOT); $rel = ''; }

$is_file = is_file($abs);
$parts = $rel === '' ? [] : explode('/', $rel);
?>
    <div class="gh-path">
      <a href="source.php">.<code><?php if ($rel==='') echo 'source'; ?></code></a>
      <?php $acc=''; foreach ($parts as $p) { $acc = $acc==='' ? $p : $acc.'/'.$p;
          echo ' / <a href="source.php?path=' . urlencode($acc) . '">' . esc($p) . '</a>'; } ?>
    </div>

<?php if ($is_file): /* ------------ file view ------------ */ ?>
    <div class="gh-filehead"><span class="gh-filelink" href="#"><?php echo esc(end($parts)); ?></span>
      <span class="gh-meta"><?php echo fmt_size((int)filesize($abs)); ?> bytes</span></div>
    <pre class="gh-code"><code><?php
        $fc = @file_get_contents($abs);
        echo esc(substr($fc, 0, 400000)); ?></code></pre>
<?php else: /* ------------ directory listing ------------ */ ?>
    <?php
    $dirs = []; $files = [];
    foreach (scandir($abs) as $e) {
        if ($e === '.' || $e === '..') continue;
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
          $href = $isD ? 'source.php?path=' . urlencode($sub) : 'source.php?path=' . urlencode($sub);
          $commit = '';
          if ($git_ok) { $commit = sh('git -C ' . escapeshellarg($ROOT) . ' log -1 --format=%h|%an|%ad --date=short -- ' . escapeshellarg($sub) . ' 2>/dev/null'); }
          [$ch, $ca, $cd] = array_pad(explode('|', $commit, 3), 3, '');
      ?>
        <tr>
          <td class="gh-name">
            <a class="<?php echo $isD ? 'gh-dir' : 'gh-file'; ?>" href="<?php echo $href; ?>">
              <span class="gh-ico"><?php echo $isD ? '&#128193;' : '&#128196;'; ?></span> <?php echo esc($name); ?>
            </a>
          </td>
          <td class="gh-c"><code><?php echo esc($ch); ?></code> <?php echo esc($ca); ?></td>
          <td class="gh-c"><?php echo esc($cd); ?></td>
          <td class="gh-c"><?php echo $isD ? '—' : fmt_size((int)@filesize($abs . '/' . $name)); ?></td>
        </tr>
      <?php endforeach; ?>
      </tbody>
    </table>

    <?php if ($readme): ?>
    <div class="gh-readme"><div class="gh-readme-t">README</div><div class="mdbody"><?php echo md(@file_get_contents($readme)); ?></div></div>
    <?php endif; ?>
<?php endif; ?>
  </div>
</section>

<?php pmm_footer(); ?>
