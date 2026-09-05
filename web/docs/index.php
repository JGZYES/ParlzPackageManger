<?php
/* PMM docs site — renders sibling .md files with the shared md() renderer.
 * URL: docs/index.php?page=<name> (defaults to "index"). */
define('PMM_SITE', 1);
require dirname(__DIR__) . '/_common.php';

/* doc pages: [slug, title] */
$pages = [
    'index'     => '快速开始',
    'install'   => '安装',
    'download'  => '下载',
    'features'  => '核心特性',
    'cli'       => '命令参考',
    'mirror'    => '镜像源',
    'translate' => '翻译 / 语言包',
    'source'    => '源码',
];
$page = isset($_GET['page']) ? (string)$_GET['page'] : 'index';
if (!isset($pages[$page])) $page = 'index';
$title = $pages[$page];

$mdPath = __DIR__ . '/' . $page . '.md';
$body = '';
if (is_file($mdPath)) {
    $body = (string)file_get_contents($mdPath);
    $body = str_replace("\r", "", $body);   // md() regex hard-codes \n
    $body = md($body);
}

pmm_header('docs', '文档 · ' . $title);
pmm_page_open('DOCS', $title);
?>
<section class="section">
  <div class="wrap">
    <nav class="docs-nav">
      <?php foreach ($pages as $slug => $t): ?>
        <a class="docs-link<?php echo $slug === $page ? ' active' : ''; ?>" href="index.php?page=<?php echo $slug; ?>"><?php echo htmlspecialchars($t); ?></a>
      <?php endforeach; ?>
    </nav>
    <div class="docs-body"><?php echo $body; ?></div>
  </div>
</section>
<?php pmm_footer(); ?>
