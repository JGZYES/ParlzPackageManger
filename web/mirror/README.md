# PMM 镜像仓库（mirror/）

这是 ParlzPackageManger (PMM) 的**软件包镜像**：一个"目录+索引"式包仓库，
像清华 apt 镜像那样，可被浏览器查看、也可供 `pmm`/`pdm` 直接下载安装。

## 内容

```
mirror/
├── README.md                # 本说明
├── index.php                # 只读 PHP 包浏览页（索引 + 详情 + 下载）
└── packages/                # 实际包文件 + registry 索引
    ├── <包>.json            # latest 指针（含 variants）
    └── <包>/<版本>-<平台>-<架构>.pdm/.json
```

已收录的包（`packages/` 下）：

| 包 | 版本 | 平台 / 架构 | 文件 |
|---|---|---|---|
| nodejs | 24.20.0 | windows/amd64, linux/amd64 | `nodejs/24.20.0-windows.pdm`, `24.20.0-linux.pdm` |
| git | 2.55.0.5 | windows/amd64 | `git/2.55.0.5-windows.pdm` |
| pmm | 0.1.1 | windows/amd64, linux/amd64, linux/aarch64 | `pmm/0.1.1-*.pdm` |
| pureftpd | 1.0.51 | linux | `pureftpd/1.0.51-linux.pdm` |

## 查看 / 下载

把本目录用 PHP 服务起来即可（不要用 router 参数，让 `.pdm` 走静态文件）：

```sh
cd mirror && php -S 0.0.0.0:8080
```

- 打开 `http://host:8080/` → 列出镜像目录下所有文件（点击即下载）
- `http://host:8080/packages/<包>/<版本>-<平台>.pdm` → 直接下载文件

Apache / Nginx 同理：`DirectoryIndex index.php`，`.pdm` 走静态文件即可。

## 客户端使用

把镜像加入客户端并安装：

```ini
# ~/.pmm/mirror.ini（Windows: %USERPROFILE%\.pmm\mirror.ini；或 -pd 后 D:\.pmm\mirror.ini）
[main]
registry = https://pmm.parlz.com/mirror/dists
priority = 1

[sz]
registry = https://sz.pmm.parlz.com/mirror/dists
priority = 20
```

```sh
pmm install nodejs        # 自动识别本机 os/arch，从镜像下载对应平台
pmm install pmm           # 安装 PMM 工具（含 pmm/pdm）
pmm self-update           # 更新 pmm（自动选平台/架构）
```

也可直接 `pmm install ./packages/nodejs/24.20.0-windows.pdm` 安装本地包。

## 数据格式

- `packages/<包>.json` — latest 指针：`{ name, version, os, arch, variants:[...] }`
- `packages/<包>/<版本>-<平台>-<架构>.pdm` — 包文件（`bin/` 内的可执行）
- `packages/<包>/<版本>-<平台>-<架构>.json` — 该变体元数据（url / sha256 / size）

`pmm install` 会按当前系统的 os+arch + 版本约束，在 `variants` 里选最高版本下载。
