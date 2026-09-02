# ParlzPackageManager (PMM)

一个用 **C 语言**从零编写的跨平台包管理器，支持 **Windows / Linux / macOS**。

像 apt 一样多用镜像源，也能直接从 **GitHub / GitLab / Gitea / Forgejo** 的 Release 安装；自带 `.pdm` 打包格式、依赖解析、并发分片下载、SHA-256 / SHA-1 校验。

> 官网：<https://pmm.parlz.com> · 镜像站：<https://sz.pmm.parlz.com/mirror>（深圳）/ <https://pmm.parlz.com/mirror>（香港）

## 特性

- **apt 式多镜像源**：`mirror.ini` / `mirror.conf`，按优先级自动回退，默认带 `sz`（深圳）与 `main`（香港）。
- **git Release 安装**：`pmm install --git repo`，支持 GitHub / GitLab / Gitea / Forgejo；`repo@tag` 或 `-b branch` 安装指定版本。
- **`.pdm` 打包**：`pmm pack <dir>` 生成含 `control.tar.gz` / `data.tar.gz` / `sha256sums` 的归档（deb 思路）。
- **依赖解析**：`pdm-control` 的 `Depends:`，registry 的 `"depends": [...]`，安装时递归解析并按版本约束安装。
- **并发分片 + 断点续传**：大文件 4 路 Range 并行、逐片续传、拼接校验；失败回退单流。进度条含已用/剩余/速度。
- **哈希校验**：内置 SHA-256 / SHA-1（FIPS 180-4），下载后强校验；`pmm verify <file>` 随时验证。
- **本地包安装**：`-dpkg x.deb`（Linux）、`-msi x.msi`（Windows），支持 `.rpm/.zip/.tar.gz/.exe/.dmg`，直接装文件或 URL。
- **搜索 / 信息 / 缓存**：`pmm search`、`pmm info`、`pmm cache clean`、`--no-cache`。

## 安装

```bash
# Linux / macOS 一键安装
curl -sSL https://pmm.parlz.com/install.sh | bash

# Linux .deb
sudo apt install ./pmm_0.2.9_amd64.deb
# Linux .rpm
sudo rpm -Uvh pmm-0.2.9.x86_64.rpm
```

Windows：下载 `pmm-0.2.9-windows-amd64.zip`，解压后把 `pmm.exe` 所在目录加入 `PATH`。

## 使用

```bash
pmm search nodejs                 # 在镜像里搜包
pmm install nodejs                # 安装（自动解析依赖）
pmm install nodejs==24.20.0       # 指定版本
pmm install --git JGZYES/ParlzPackageManger@v0.2.9   # git release 指定 tag
pmm info nodejs                   # registry / 本地信息
pmm verify pmm.pdm                # 校验哈希
pmm list                          # 已装列表
pmm remove nodejs                 # 卸载
pmm pack ./myapp myapp.pdm        # 打包 .pdm
```

## 构建

Linux / macOS：

```bash
make
```

Windows（需 MinGW gcc）：

```bat
build.bat
```

源码结构：

```
src/
├── main.c      # CLI 分发
├── pmm.c       # 配置 / 目录 / PATH / 注册表
├── http.c      # HTTP 下载（并行分片 + 进度）
├── repo.c      # git host 适配 + 资产选择
├── install.c   # 安装分发 + 依赖解析
├── pdm.c       # .pdm 打包 / 安装
├── ini.c / json.c  # 配置解析
└── sha256.c / sha1.c  # 哈希
```

## 相关

- 官网：[pmm.parlz.com](https://pmm.parlz.com)
- 源码浏览：[git.php](https://pmm.parlz.com/git.php)
- 更新日志：[CHANGELOG](CHANGELOG/)
- 许可：GNU GPL-3.0
