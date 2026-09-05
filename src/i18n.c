/* i18n.c - .pjson language-pack (translation) support */
#include "i18n.h"
#include "json.h"
#include "pmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static JsonValue *g_lang = NULL;
static JsonValue *g_builtin = NULL;
static char g_locale[64] = "";

/* Detect locale from LANG / LC_ALL env, else default "zh-CN". */
static void detect_locale(void) {
    const char *lc = getenv("LC_ALL");
    if (!lc || !*lc) lc = getenv("LANG");
    if (!lc || !*lc) lc = "zh-CN";
    /* Normalise: 'en_US.UTF-8' -> 'en-US' */
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", lc);
    char *dot = strchr(buf, '.');
    if (dot) *dot = '\0';
    char *underscore = strchr(buf, '_');
    if (underscore) *underscore = '-';
    if (g_locale[0] == '\0') snprintf(g_locale, sizeof(g_locale), "%s", buf);
}

void pmm_lang_set_locale(const char *locale) {
    if (!locale || !*locale) { g_locale[0] = '\0'; return; }
    snprintf(g_locale, sizeof(g_locale), "%s", locale);
}

const char *pmm_lang_locale(void) {
    if (g_locale[0] == '\0') detect_locale();
    return g_locale;
}

int pmm_lang_load(const char *path) {
    JsonValue *root = json_parse_file(path);
    if (!root) return -1;
    if (g_lang) json_free(g_lang);
    g_lang = root;
    return 0;
}

/* Built-in fallback so PMM is usable even before any .pjson is installed. */
static const char *kBuiltinZh =
"{\"help.usage\":\"用法:\",\"help.commands\":\"命令:\",\"help.options\":\"选项:\","
"\"cmd.install\":\"install\",\"cmd.remove\":\"remove\",\"cmd.update\":\"update\","
"\"cmd.upgrade\":\"upgrade\",\"cmd.mirror\":\"mirror\",\"cmd.search\":\"search\","
"\"msg.looking-up\":\"在镜像 %s 中查找 %s\",\"msg.selected\":\"已选择 %s@%s (os=%s arch=%s)\","
"\"msg.downloading\":\"下载 %s\",\"msg.downloaded\":\"已下载 %s\","
"\"msg.hash-sha256\":\"sha256: %s\",\"msg.hash-sha1\":\"sha1: %s\","
"\"msg.installing\":\"正在安装 %s ...\",\"msg.installed\":\"已安装 %s\","
"\"msg.checksum-ok\":\"%s 校验 OK: %s\",\"msg.checksum-mismatch\":\"校验不匹配 (%s)!\","
"\"msg.resolving-dep\":\"正在解析依赖 %s%s%s\",\"msg.dep-satisfied\":\"依赖 %s%s%s 已装/被提供,跳过\",\"msg.registry-ok\":\"注册表 sha256 OK: %s\",\"msg.cache-cleared\":\"已清理缓存: %d 个文件，释放 %.1f KB\",\"msg.doctor-issues\":\"doctor: 发现 %d 个问题\",\"msg.self-up-to-date\":\"当前 v%s 已是最新（v%s），跳过升级\","
"\"msg.path-exists\":\"PATH 已包含 PMM 目录\",\"msg.err.registry-not-found\":\"找不到软件包 '%s'\","
"\"msg.pmm-self-replaced\":\"pmm 已更新到 %s\",\"msg.pmm-self-hint\":\"无法覆盖 %s（需 root/管理员）;新版本在 %s。请将其加入 PATH 并在当前 shell 运行 hash -r\","
"\"msg.err.usage\":\"用法\",\"msg.err.unknown-cmd\":\"未知命令 '%s' (试试 'pmm help')\","
"\"msg.err.download-failed\":\"下载 %s 失败\",\"msg.err.invalid-locale\":\"无效语言 %s\","
"\"msg.err.not-found\":\"找不到软件包 '%s'\",\"msg.err.failed-install\":\"安装 %s 失败\",\"msg.err.conflict\":\"无法安装 %s:与已装的 %s 冲突\","
"\"msg.err.no-registry-mirror\":\"没有配置注册表镜像\",\"msg.err.cannot-write\":\"无法写入 %s\","
"\"desc.install\":\"安装\",\"desc.remove\":\"卸载\",\"desc.update\":\"刷新索引\","
"\"desc.upgrade\":\"升级\",\"desc.mirror\":\"镜像源\",\"desc.search\":\"搜索\","
"\"desc.info\":\"详情\",\"desc.self-update\":\"更新自身\",\"desc.clean\":\"清理缓存\",\"desc.doctor\":\"诊断\",\"desc.help\":\"帮助\","
"\"opt.p-drive\":\"安装盘符\",\"opt.no-color\":\"禁用颜色\",\"opt.quiet\":\"静默\","
"\"opt.verbose\":\"详情\"}";


static const char *kBuiltinEn =
"{\"help.banner\":\"PMM %s (%s)\",\"help.usage\":\"usage:\",\"help.commands\":\"commands:\",\"help.options\":\"options:\",\"help.common\":\"common commands:\",\"cmd.list\":\"list\",\"cmd.info\":\"info\",\"cmd.search\":\"search\",\"cmd.install\":\"install\",\"cmd.remove\":\"remove\",\"cmd.update\":\"update\",\"cmd.upgrade\":\"upgrade\",\"cmd.mirror\":\"mirror\",\"cmd.self-update\":\"self-update\",\"cmd.version\":\"version\",\"cmd.help\":\"help\",\"desc.list\":\"list packages by name\",\"desc.info\":\"show detailed info for a package\",\"desc.search\":\"search packages by keyword\",\"desc.install\":\"install a package by name\",\"desc.remove\":\"uninstall a package by name\",\"desc.update\":\"refresh the registry index (apt-style)\",\"desc.upgrade\":\"upgrade all installed packages\",\"desc.mirror\":\"list / manage mirrors\",\"desc.self-update\":\"update the pmm tool itself\",\"desc.version\":\"show version\",\"desc.help\":\"show help\",\"opt.p-drive\":\"install under <DRIVE>:\\\\\\\\.pmm\",\"opt.no-color\":\"omit ANSI colours on a terminal (PMM_NO_COLOR=1 too)\",\"opt.quiet\":\"only print errors/warnings (suppress info/success)\",\"opt.verbose\":\"print extra detail\",\"msg.looking-up\":\"looking up %s in mirror %s\",\"msg.selected\":\"selected %s@%s (os=%s arch=%s)\",\"msg.downloading\":\"downloading %s%s\",\"msg.downloaded\":\"downloaded %s\",\"msg.hash-sha256\":\"sha256: %s\",\"msg.hash-sha1\":\"sha1:   %s\",\"msg.sha256\":\"sha256: %s  %s\",\"msg.sha1\":\"sha1:   %s  %s\",\"msg.installing\":\"installing %s ...\",\"msg.installing-file\":\"installing local file %s\",\"msg.installed\":\"installed %s\",\"msg.removed\":\"removed %s (%d files)\",\"msg.packed\":\"packed %s (%s %s) -> %s\",\"msg.checksum-ok\":\"%s checksum OK: %s\",\"msg.checksum-mismatch\":\"CHECKSUM MISMATCH (%s)!\\n  expected: %s\\n  actual:   %s\",\"msg.registry-ok\":\"registry sha256 OK: %s\",\"msg.registry-not-found\":\"package '%s' not found in any registry mirror\",\"msg.no-version\":\"no version of '%s' for os=%s arch=%s satisfying '%s'\",\"msg.up-to-date\":\"%s %s is up to date (latest %s)\",\"msg.upgrading\":\"%s: %s -> %s\",\"msg.upgraded\":\"upgraded %d package(s).\",\"msg.no-installed\":\"no installed packages.\",\"msg.registry-empty\":\"registry index is empty.\",\"msg.registry-updated\":\"updated registry index: %d/%d packages cached.\",\"msg.skipping-no-registry\":\"skipping %s: no registry entry\",\"msg.no-match\":\"no package matches '%s'\",\"msg.description\":\"description: %s\",\"msg.cleaning-cache\":\"cleaning cache %s\",\"msg.drive-set\":\"install drive set to %c:\",\"msg.path-set\":\"install path set to %s\",\"msg.path-adding\":\"adding to PATH: %s\",\"msg.path-exists\":\"PATH already contains PMM dirs\",\"msg.path-updated\":\"PATH updated for new shells (reopen a terminal or run: setx)  (user-level)\",\"msg.path-reload\":\"updated %s (reload your shell)\",\"msg.registered\":\"registered '%s' %s in Installed Apps (uninstall via pmm remove %s)\",\"msg.unregistered\":\"removed '%s' from Installed Apps\",\"msg.mirror-added\":\"mirror '%s' added\",\"msg.mirror-active\":\"active mirror set to '%s'\",\"msg.mirror-removed\":\"mirror '%s' removed\",\"msg.lang-set\":\"language set to '%s'\",\"msg.setting-set\":\"set %s = %s\",\"msg.empty-path\":\"empty file path\",\"msg.verifying-native\":\"verifying %s is a native %s binary...\",\"msg.resolving-dep\":\"resolving dependency %s%s%s\",\"msg.err.usage\":\"usage:\",\"msg.err.not-found\":\"package '%s' not found\",\"msg.err.not-installed\":\"package '%s' is not installed\",\"msg.err.empty-path\":\"empty file path\",\"msg.err.registry-not-found\":\"package '%s' not found in any registry mirror\",\"msg.err.no-suitable-asset\":\"no suitable %s asset found in latest release of %s\",\"msg.err.unknown-cmd\":\"unknown command '%s' (try 'pmm help')\",\"msg.err.failed-install\":\"failed to install %s\",\"msg.err.cannot-write\":\"cannot write %s\",\"msg.err.cannot-read\":\"cannot read %s\",\"msg.err.cannot-open\":\"cannot open file: %s\",\"msg.err.invalid-locale\":\"invalid language pack: %s\",\"msg.err.download-failed\":\"failed to download %s\",\"msg.err.no-registry-mirror\":\"no registry mirror configured. Add to ~/.pmm/mirror.ini\",\"msg.err.no-mirror\":\"no mirrors configured (use: pmm mirror add)\",\"msg.err.no-mirror-file\":\"no mirror file\",\"msg.err.no-registry-index\":\"no registry index (packages.json) available\",\"msg.err.bad-registry-index\":\"bad registry index\",\"msg.err.bad-entry\":\"bad registry entry '%s'\",\"msg.err.bad-registry-entry\":\"bad registry entry for '%s' (%s)\",\"msg.err.checksum-mismatch-file\":\"checksum mismatch for %s in %s\",\"msg.err.no-control\":\"%s has no pdm-control file\",\"msg.err.no-package\":\"pdm-control missing 'Package:' field\",\"msg.err.tar-control\":\"tar (control) failed\",\"msg.err.tar-data\":\"tar (data) failed\",\"msg.err.not-pdm\":\"'%s' is not a .pdm file\",\"msg.err.no-tar\":\"cannot run tar\",\"msg.err.bad-archive\":\"not a valid .pdm archive: %s\",\"msg.err.no-control-tar\":\"missing control.tar.gz in %s\",\"msg.err.extract\":\"data extraction failed\",\"msg.err.oom\":\"out of memory\",\"msg.err.json-parse\":\"JSON parse error: %s\",\"msg.err.unknown-host\":\"unknown host '%s'\",\"msg.err.checksum-refuse\":\"refusing to install: checksum verification failed\",\"msg.err.registry-entry-no-url\":\"registry entry for '%s' has no url\",\"msg.err.rpm-stage\":\"cannot create rpm stage dir: %s\",\"msg.err.rpm-decompress\":\"rpm payload decompress/cpio failed (need %s)\",\"msg.err.rpm-copy\":\"failed to copy rpm payload into /\",\"msg.warn.parse-checksum\":\"warning: could not parse %s checksum file\",\"msg.warn.download-retry\":\"download failed, trying next source...\",\"msg.warn.not-usable\":\"%s is not a usable %s binary; will fall back\",\"msg.warn.no-checksum\":\"warning: no sha256sums member, skipping integrity check\",\"msg.warn.no-filelist\":\"warning: could not record file list\",\"msg.warn.no-register\":\"warning: could not register '%s' in Installed Apps\",\"msg.api-unreachable\":\"could not reach a known release API for %s\",\"msg.retrying-asset\":\"retrying with %s (%s)\",\"mirror.sources\":\"mirrors:\",\"mirror.checking\":\"checking registry mirrors...\",\"mirror.result\":\"result: %d/%d sources reachable\",\"lang.packs-in\":\"language packs in %s:\",\"desc.clean\":\"clean cache\",\"msg.dep-satisfied\":\"dependency %s%s%s already installed/provided, skipped\",\"msg.err.conflict\":\"cannot install %s: conflicts with installed %s\",\"msg.cache-cleared\":\"cache cleaned: %d files, freed %.1f KB\",\"msg.self-up-to-date\":\"already up to date (v%s, latest v%s), skipping upgrade\",\"desc.doctor\":\"diagnose\",\"msg.doctor-issues\":\"doctor: found %d issue(s)\"}";

/* Which built-in fallback matches the current locale? en -> en-US, else zh-CN. */
static const char *builtin_source(void) {
    const char *loc = pmm_lang_locale();
    if (loc && strncasecmp(loc, "en", 2) == 0) return kBuiltinEn;
    return kBuiltinZh;
}

int pmm_lang_load_active(void) {
    char dir[1024];
    pmm_config_dir(dir, sizeof(dir));
    char path[1400];
    snprintf(path, sizeof(path), "%s/lang/%s.pjson", dir, pmm_lang_locale());
    if (pmm_lang_load(path) == 0) return 0;
    /* No pack on disk: fall back to the built-in table for the current locale
     * (en-US / zh-CN) so output is readable instead of raw msg.xxx keys. */
    JsonValue *root = json_parse(builtin_source());
    if (root) { if (g_lang) json_free(g_lang); g_lang = root; return 0; }
    return 1;
}

const char *pmm_tr(const char *key) {
    /* Look up in the loaded pack first, then fall back to the built-in zh-CN
     * table so a stale .pjson never shows a raw "msg.xxx" key. */
    if (g_lang) {
        const char *t = json_str(g_lang, key);
        if (t) return t;
    }
    if (!g_builtin) g_builtin = json_parse(builtin_source());
    if (g_builtin) {
        const char *b = json_str(g_builtin, key);
        if (b) return b;
    }
    return key;
}

const char *pmm_tr_fmt(const char *key, ...) {
    /* pmm_tr returns a pointer into the parsed JSON (or the key). We safe-copy
     * the format string into a static buffer, then vsnprintf + args. */
    static char out[2048];
    const char *fmt = pmm_tr(key);
    /* copy fmt into a local buffer so the static out doesn't alias it */
    char fmtbuf[2048];
    snprintf(fmtbuf, sizeof(fmtbuf), "%s", fmt);
    va_list ap; va_start(ap, key);
    vsnprintf(out, sizeof(out), fmtbuf, ap);
    va_end(ap);
    return out;
}
