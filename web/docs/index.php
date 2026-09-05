<?php
/* PMM docs site — left sidebar directory + right rendered .md content.
 * URL: docs/index.php?page=<name> (defaults to "index"). */
define('PMM_SITE', 1);
require dirname(__DIR__) . '/_common.php';

/* doc pages: [slug, title] — grouped sections */
$groups = [
    '开始' => [
        'index'   => '快速开始',
        'install' => '安装',
        'download'=> '下载',
    ],
    '了解' => [
        'features' => '核心特性',
        'cli'      => '命令参考',
        'mirror'   => '镜像源',
    ],
    '社区' => [
        'translate' => '翻译 / 语言包',
        'source'    => '源码',
    ],
];
// flatten for page lookup
$pages = [];
foreach ($groups as $g => $list) foreach ($list as $slug => $t) $pages[$slug] = $t;

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
?>
<section class="docs-layout">
  <aside class="docs-sidebar">
    <div class="docs-sidebar-brand"><span>PMM 文档</span></div>
    <nav class="docs-tree">
      <?php foreach ($groups as $g => $list): ?>
        <div class="docs-group"><?php echo htmlspecialchars($g); ?></div>
        <?php foreach ($list as $slug => $t): ?>
          <a class="docs-link<?php echo $slug === $page ? ' active' : ''; ?>" href="index.php?page=<?php echo $slug; ?>"><?php echo htmlspecialchars($t); ?></a>
        <?php endforeach; ?>
      <?php endforeach; ?>
    </nav>
  </aside>
  <div class="docs-content">
    <div class="docs-crumb">文档 / <?php echo htmlspecialchars($title); ?></div>
    <div class="docs-body"><?php echo $body; ?></div>
  </div>
</section>
<?php pmm_footer(); ?>
