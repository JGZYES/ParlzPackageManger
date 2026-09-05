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
"\"msg.err.download-failed\":\"下载语言包 %s 失败\",\"msg.err.invalid-locale\":\"无效语言 %s\","
"\"msg.err.not-found\":\"找不到软件包 '%s'\",\"msg.err.failed-install\":\"安装 %s 失败\",\"msg.err.conflict\":\"无法安装 %s:与已装的 %s 冲突\","
"\"msg.err.no-registry-mirror\":\"没有配置注册表镜像\",\"msg.err.cannot-write\":\"无法写入 %s\","
"\"desc.install\":\"安装\",\"desc.remove\":\"卸载\",\"desc.update\":\"刷新索引\","
"\"desc.upgrade\":\"升级\",\"desc.mirror\":\"镜像源\",\"desc.search\":\"搜索\","
"\"desc.info\":\"详情\",\"desc.self-update\":\"更新自身\",\"desc.clean\":\"清理缓存\",\"desc.doctor\":\"诊断\",\"desc.help\":\"帮助\","
"\"opt.p-drive\":\"安装盘符\",\"opt.no-color\":\"禁用颜色\",\"opt.quiet\":\"静默\","
"\"opt.verbose\":\"详情\"}";

int pmm_lang_load_active(void) {
    char dir[1024];
    pmm_config_dir(dir, sizeof(dir));
    char path[1400];
    snprintf(path, sizeof(path), "%s/lang/%s.pjson", dir, pmm_lang_locale());
    if (pmm_lang_load(path) == 0) return 0;
    /* No pack on disk: fall back to the built-in zh-CN so output is readable
     * instead of printing raw keys (msg.xxx). */
    JsonValue *root = json_parse(kBuiltinZh);
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
    if (!g_builtin) g_builtin = json_parse(kBuiltinZh);
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
