/* pmm_err.h — stable PMM error codes.
 *
 * Every user-facing error carries one of these codes so scripts/tools and users
 * can reliably identify it. Pair it with a human hint via pmm_error_c(). */
#ifndef PMM_ERR_H
#define PMM_ERR_H

enum {
    PMM_E_OK = 0,          /* 成功 */
    PMM_E_USAGE = 1,       /* 用法/参数错误 */
    PMM_E_NOT_FOUND = 2,   /* 找不到软件包/文件 */
    PMM_E_DOWNLOAD = 3,    /* 下载失败 */
    PMM_E_NETWORK = 4,     /* 网络/连接问题 */
    PMM_E_NO_MIRROR = 5,   /* 未配置/无可用镜像 */
    PMM_E_REGISTRY = 6,    /* 注册表条目异常 */
    PMM_E_CHECKSUM = 7,    /* 校验和不匹配/缺失 */
    PMM_E_EXTRACT = 8,     /* 解包/解压失败 */
    PMM_E_CONFLICT = 9,    /* 依赖/冲突 */
    PMM_E_NO_TAR = 10,     /* 缺少 tar */
    PMM_E_NO_ROOT = 11,    /* 需要 root/权限不足 */
    PMM_E_CACHE = 12,      /* 缓存问题(未缓存/写入) */
    PMM_E_LANG = 13,       /* 语言包错误 */
    PMM_E_INTERNAL = 99    /* 内部/未知错误 */
};

#endif
