/* pmu.c — PMU : package upload / publish client for the PMM registry.
 *
 *   pmu register <email> <password>   create an account (local arithmetic captcha)
 *   pmu login    <email> <password>   log in, store a session token
 *   pmu logout                        revoke the stored token
 *   pmu whoami                        show logged-in email + server
 *   pmu ./foo.pdm                     publish a package (alias: pmu publish ./foo.pdm)
 *   pmu help | -h
 *
 * Talks to the PHP service under {server}/ (default https://pmm.parlz.com/pmu).
 * Config is stored in ~/.pmu/config (Windows: D:\.pmu\config).
 */
#include "out.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define PMU_MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#include <sys/stat.h>
#define PMU_MKDIR(p) mkdir((p), 0755)
#endif

static int g_no_color = 0;

static const char *pmu_server_default = "https://pmm.parlz.com/pmu";

/* ---- config dir / file: ~/.pmu  (Windows D:\.pmu) ---- */
static void pmu_config_dir(char *buf, size_t sz) {
#ifdef _WIN32
    snprintf(buf, sz, "D:\\.pmu");
#else
    const char *home = getenv("HOME");
    snprintf(buf, sz, "%s/.pmu", home && *home ? home : ".");
#endif
}

static char g_cfg[1400];
static char g_server[2048] = "";
static char g_token[128] = "";
static char g_email[256] = "";

static void cfg_init(void) {
    char dir[1024];
    pmu_config_dir(dir, sizeof(dir));
    PMU_MKDIR(dir);
    snprintf(g_cfg, sizeof(g_cfg), "%s/config", dir);
    snprintf(g_server, sizeof(g_server), "%s", pmu_server_default);
    FILE *f = fopen(g_cfg, "r");
    if (f) {
        char line[2400];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            char key[64]; size_t kl = (size_t)(eq - line);
            while (kl && (line[kl-1] == ' ' || line[kl-1] == '\t')) kl--;
            if (kl >= sizeof(key)) kl = sizeof(key) - 1;
            memcpy(key, line, kl); key[kl] = 0;
            char *val = eq + 1;
            while (*val == ' ' || *val == '\t') val++;
            char *e = val + strlen(val);
            while (e > val && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
            if (strcmp(key, "server") == 0) snprintf(g_server, sizeof(g_server), "%s", val);
            else if (strcmp(key, "token") == 0) snprintf(g_token, sizeof(g_token), "%s", val);
            else if (strcmp(key, "email") == 0) snprintf(g_email, sizeof(g_email), "%s", val);
        }
        fclose(f);
    }
}
static void cfg_save_token(const char *token, const char *email) {
    snprintf(g_token, sizeof(g_token), "%s", token ? token : "");
    snprintf(g_email, sizeof(g_email), "%s", email ? email : "");
    FILE *f = fopen(g_cfg, "w");
    if (!f) { pmm_error("无法写入 %s\n", g_cfg); return; }
    fprintf(f, "server = %s\n", g_server);
    fprintf(f, "token  = %s\n", g_token);
    fprintf(f, "email  = %s\n", g_email);
    fclose(f);
}

/* ---- HTTP POST via curl ---- */
static int http_post(const char *url, const char *auth, const char *body,
                     const char *file, char *out, size_t outs) {
    char cmd[9000];
    int i = 0;
    if (file) { i += snprintf(cmd + i, sizeof(cmd) - (size_t)i, "curl -s -X POST %s --data-binary @'%s' '%s'",
                              auth ? auth : "", file, url); }
    else      { i += snprintf(cmd + i, sizeof(cmd) - (size_t)i, "curl -s -X POST %s -H 'Content-Type: application/json' --data-binary '%s' '%s'",
                              auth ? auth : "", body ? body : "", url); }
    if (i < 0 || (size_t)i >= sizeof(cmd)) { pmm_error("请求过长\n"); return -1; }
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    size_t got = fread(out, 1, outs - 1, f);
    out[got] = '\0';
    pclose(f);
    return (int)got;
}

/* ---- tiny JSON getters (server returns {"ok":true,...,"error":".."}) ---- */
static const char *js_str(const char *body, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(body, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return NULL;
    p++;
    const char *end = strchr(p, '"');
    if (!end) return NULL;
    return p; /* caller reads up to end (we return pointer; use length) */
}
static int js_ok(const char *body) { return strstr(body, "\"ok\":true") != NULL; }
static const char *js_err(const char *body) {
    const char *p = strstr(body, "\"error\":");
    if (!p) return NULL;
    p += 8;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') p++;
    return p;
}
static const char *js_val(const char *body, const char *key, char *buf, size_t sz) {
    const char *p = js_str(body, key);
    if (!p) return NULL;
    const char *end = strchr(p, '"');
    size_t n = end ? (size_t)(end - p) : strlen(p);
    if (n >= sz) n = sz - 1;
    memcpy(buf, p, n); buf[n] = 0;
    return buf;
}
static void js_err_print(const char *body) {
    const char *e = js_err(body);
    if (e) {
        char buf[512]; size_t n = strlen(e);
        const char *q = strchr(e, '"');
        if (q && ((size_t)(q - e)) < n) n = (size_t)(q - e);
        if (n >= sizeof(buf)) n = sizeof(buf) - 1;
        memcpy(buf, e, n); buf[n] = 0;
        pmm_error("%s\n", buf);
    } else {
        pmm_error("%s\n", body ? body : "(no response)");
    }
}

/* ---- local arithmetic human verification ---- */
static int captcha(void) {
    srand((unsigned)time(NULL));
    int a = rand() % 30 + 20;      /* 20..49 */
    int b = rand() % 10 + 3;       /* 3..12 */
    int op = rand() % 3;           /* 0=+ 1=- 2=× */
    int ans;
    const char *sym;
    if (op == 0) { ans = a + b; sym = "+"; }
    else if (op == 1) { ans = a - b; sym = "-"; }
    else { ans = a * b; sym = "\xc3\x97"; } /* × */
    printf("人机验证: %d %s %d = ?\n答案: ", a, sym, b);
    fflush(stdout);
    char in[64];
    if (!fgets(in, sizeof(in), stdin)) return 0;
    if (atoi(in) != ans) { pmm_error("验证错误\n"); return 0; }
    pmm_success("人机验证通过\n");
    return 1;
}

/* ---- parse Package/Version/Architecture out of a .pdm's control ---- */
static void pdm_meta(const char *path, char *pkg, size_t pkgsz,
                     char *ver, size_t versz, char *arch, size_t archsz) {
    pkg[0] = ver[0] = arch[0] = 0;
    char cmd[1800];
    snprintf(cmd, sizeof(cmd), "tar -xOf '%s' control.tar.gz 2>/dev/null | tar -xzOf - pdm-control 2>/dev/null", path);
    FILE *f = popen(cmd, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *e = line + strlen(line);
        while (e > line && (e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
        if (strncmp(line, "Package:", 8) == 0 && !pkg[0]) snprintf(pkg, pkgsz, "%s", line + 8);
        else if (strncmp(line, "Version:", 8) == 0 && !ver[0]) snprintf(ver, versz, "%s", line + 8);
        else if (strncmp(line, "Architecture:", 13) == 0 && !arch[0]) snprintf(arch, archsz, "%s", line + 13);
    }
    pclose(f);
    /* left-trim spaces */
    for (char *s = pkg; *s && (*s == ' ' || *s == '\t'); s++) memmove(s, s + 1, strlen(s));
    for (char *s = ver; *s && (*s == ' ' || *s == '\t'); s++) memmove(s, s + 1, strlen(s));
    for (char *s = arch; *s && (*s == ' ' || *s == '\t'); s++) memmove(s, s + 1, strlen(s));
}

static void usage(void) {
    printf("pmu — 发布 PMM 包\n\n"
           "用法:\n"
           "  pmu register <email> <password>   注册账号(本地算术人机验证)\n"
           "  pmu login    <email> <password>   登录并保存 token\n"
           "  pmu logout                         撤销并清除本地 token\n"
           "  pmu whoami                         当前登录邮箱 + 服务器\n"
           "  pmu ./xxxx.pdm                     发布包(自动生成 json)\n"
           "  pmu help | -h                      帮助\n\n"
           "服务端默认: %s (可用 pmu 配置 server= 覆盖)\n", pmu_server_default);
}

/* Detect the CPU architecture (used as the registry 'arch' field). */
static void detect_cpu_arch(char *out, size_t sz) {
    out[0] = 0;
#ifdef _WIN32
    const char *p = getenv("PROCESSOR_ARCHITECTURE");
    if (p && (strcmp(p, "ARM64") == 0 || strcmp(p, "arm64") == 0)) snprintf(out, sz, "aarch64");
    else snprintf(out, sz, "amd64");
#else
    FILE *f = popen("uname -m 2>/dev/null", "r");
    if (f) {
        char m[64];
        if (fgets(m, sizeof(m), f)) {
            char *e = m + strlen(m);
            while (e > m && (e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
            if (strcmp(m, "x86_64") == 0 || strcmp(m, "amd64") == 0) snprintf(out, sz, "amd64");
            else if (strcmp(m, "aarch64") == 0 || strcmp(m, "arm64") == 0) snprintf(out, sz, "aarch64");
            else snprintf(out, sz, "%s", m);
        }
        pclose(f);
    }
    if (!out[0]) snprintf(out, sz, "amd64");
#endif
}

int main(int argc, char **argv) {
    g_no_color = getenv("PMM_NO_COLOR") ? 1 : 0;
    cfg_init();
    if (argc < 2) { usage(); return 1; }

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) { usage(); return 0; }

    char resp[8192];
    char url[2300], auth[256] = "";

    /* ---- register ---- */
    if (strcmp(argv[1], "register") == 0) {
        if (argc < 4) { pmm_error("用法: pmu register <email> <password>\n"); return 1; }
        const char *email = argv[2], *pass = argv[3];
        if (strchr(email, '\'') || strchr(pass, '\'')) { pmm_error("邮箱/密码不能含单引号\n"); return 1; }
        if (!captcha()) return 1;
        snprintf(url, sizeof(url), "%s/register.php", g_server);
        char json[1024];
        snprintf(json, sizeof(json), "{\"email\":\"%s\",\"password\":\"%s\"}", email, pass);
        http_post(url, NULL, json, NULL, resp, sizeof(resp));
        if (js_ok(resp)) { pmm_success("注册成功: %s\n", email); cfg_save_token("", email); }
        else js_err_print(resp);
        return js_ok(resp) ? 0 : 1;
    }

    /* ---- login ---- */
    if (strcmp(argv[1], "login") == 0) {
        if (argc < 4) { pmm_error("用法: pmu login <email> <password>\n"); return 1; }
        const char *email = argv[2], *pass = argv[3];
        if (strchr(email, '\'') || strchr(pass, '\'')) { pmm_error("邮箱/密码不能含单引号\n"); return 1; }
        snprintf(url, sizeof(url), "%s/login.php", g_server);
        char json[1024];
        snprintf(json, sizeof(json), "{\"email\":\"%s\",\"password\":\"%s\"}", email, pass);
        http_post(url, NULL, json, NULL, resp, sizeof(resp));
        if (js_ok(resp)) {
            char tok[256] = "";
            js_val(resp, "token", tok, sizeof(tok));
            cfg_save_token(tok, email);
            pmm_success("登录成功: %s\n", email);
        } else js_err_print(resp);
        return js_ok(resp) ? 0 : 1;
    }

    /* ---- logout ---- */
    if (strcmp(argv[1], "logout") == 0) {
        if (g_token[0]) {
            snprintf(auth, sizeof(auth), "-H 'Authorization: Bearer %s'", g_token);
            snprintf(url, sizeof(url), "%s/logout.php", g_server);
            http_post(url, auth, "{}", NULL, resp, sizeof(resp));
        }
        cfg_save_token("", "");
        pmm_success("已登出\n");
        return 0;
    }

    /* ---- whoami ---- */
    if (strcmp(argv[1], "whoami") == 0) {
        pmm_info("服务器: %s\n", g_server);
        pmm_info("邮箱:   %s\n", g_email[0] ? g_email : "(未登录)");
        pmm_info("token:  %s\n", g_token[0] ? "(已登录)" : "(无)");
        return 0;
    }

    /* ---- publish ./x.pdm (or "publish") ---- */
    const char *file = NULL;
    if (strcmp(argv[1], "publish") == 0) file = argc > 2 ? argv[2] : NULL;
    else if (strlen(argv[1]) > 4 && strcmp(argv[1] + strlen(argv[1]) - 4, ".pdm") == 0) file = argv[1];
    if (file) {
        if (!g_token[0]) { pmm_error("请先 pmu login\n"); return 1; }
        char pkg[256], ver[128], arch[64];
        pdm_meta(file, pkg, sizeof(pkg), ver, sizeof(ver), arch, sizeof(arch));
        if (!pkg[0] || !ver[0]) { pmm_error("无法解析 %s 的 Package/Version\n", file); return 1; }
        char hex[128];
        if (pmm_sha256_file(file, hex) != 0) { pmm_error("无法计算 sha256\n"); return 1; }
        /* URL-encode description (name/version/arch/os are safe charsets) */
        char desc[512];
        snprintf(desc, sizeof(desc), "%s %s", pkg, ver);
        char enc[2100]; size_t ei = 0;
        for (const char *p = desc; *p && ei + 4 < sizeof(enc); p++) {
            if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
                *p == '-' || *p == '_' || *p == '.') enc[ei++] = *p;
            else if (*p == ' ') { enc[ei++] = '%'; enc[ei++] = '2'; enc[ei++] = '0'; }
            else { enc[ei++] = '%'; enc[ei++] = "0123456789ABCDEF"[((unsigned char)*p) >> 4];
                                enc[ei++] = "0123456789ABCDEF"[((unsigned char)*p) & 15]; }
        }
        enc[ei] = 0;
        char *sp = enc; while (*sp == ' ') sp++; /* trim leading space */
        /* os = the .pdm control's Architecture (pmm convention: linux/windows);
         * arch = the CPU arch, detected separately. */
        char os[16];
        snprintf(os, sizeof(os), "%s", arch[0] ? arch : "linux");
        if (strcmp(os, "windows") != 0 && strcmp(os, "macos") != 0 && strcmp(os, "linux") != 0)
            snprintf(os, sizeof(os), "linux");
        char cpuarch[16] = "";
        detect_cpu_arch(cpuarch, sizeof(cpuarch));
        snprintf(url, sizeof(url), "%s/publish.php?name=%s&version=%s&arch=%s&os=%s&description=%s",
                 g_server, pkg, ver, cpuarch[0] ? cpuarch : "amd64", os, sp);
        snprintf(auth, sizeof(auth), "-H 'Authorization: Bearer %s'", g_token);
        pmm_info("正在发布 %s %s ...\n", pkg, ver);
        if (http_post(url, auth, NULL, file, resp, sizeof(resp)) <= 0) { pmm_error("发布失败(无响应)\n"); return 1; }
        if (js_ok(resp)) {
            char u[2200] = ""; js_val(resp, "url", u, sizeof(u));
            pmm_success("发布成功: %s\n", u[0] ? u : file);
        } else js_err_print(resp);
        return js_ok(resp) ? 0 : 1;
    }

    pmm_error("未知命令: %s (试试 pmu help)\n", argv[1]);
    return 1;
}
