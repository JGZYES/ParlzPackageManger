# ParlzPackageManger (PMM)

> **License: GNU GPL v3** — 本项目以 GNU General Public License v3 发布，详见 `LICENSE`。

用 C 语言编写的跨平台包管理器（Windows / Linux / macOS），零第三方库依赖
（下载通过系统自带的 `curl` 完成，Windows 10+、macOS、主流 Linux 均预装）。

## 构建

```sh
make            # Linux / macOS（或任何有 make 的环境），生成 pmm 二进制
build.bat       # Windows（只需 MinGW gcc），生成 pmm.exe
make install    # 安装到 /usr/local/bin（可选）
```

产物是单文件 `pmm`（或 `pmm.exe`），无 DLL/第三方库依赖。

> 重要：Windows 构建须用**静态链接**（`-static`）。否则产物会依赖 MinGW 的
> `libmcfgthread-2.dll`，在没装 MinGW 的普通 PowerShell 里会因为找不到该 DLL 而
> **静默不输出**（加载即退出）。`Makefile`/`build.bat` 已默认加 `-static`。

## 使用

```sh
# 从镜像站（apt 式多镜像）安装
pmm install <pkg>                          # 按优先级从 registry 镜像查找并安装
pmm install <file.pdm>                     # 安装本地 .pdm 包
pmm pack <dir> [out.pdm]                   # 把文件夹打包成 .pdm（模拟 dpkg deb）
pmm pdm install|list|remove|info ...       # 管理 .pdm 包

# 从 git 托管仓库的最新 release 安装
pmm install --git https://host/owner/repo.git   # 任意 git 仓库（自动探测 API 形状）
pmm install --github owner/repo                # GitHub
pmm install --gitlab owner/repo                # GitLab
pmm install --gitea https://git.example.com/owner/repo   # Gitea/Forgejo（含私人部署）
pmm install --git owner/repo --host forgejo               # 强制指定主机类型

pmm mirror list / add <名> <源> / use <名> / remove <名>
pmm self-update                  # 从镜像安装最新版 pmm（自动识别本机 os/arch）
pmm list
pmm help / version
pmm install <pkg> -pd          # 安装到 D:\.pmm（可换成 -pc 回到 C 盘，-px 任意盘）
```

## 换安装盘（-p<drive>，避免塞爆 C 盘）

**默认安装在 `D:\.pmm`**（config + cache + bin + root 全在这里，避免占满 C 盘）。
用 `-p<盘符>` 可以改变 PMM 的**整个家目录**：

- 默认（不带 `-p`）→ `D:\.pmm\`（node 等 .exe 装到 `D:\.pmm\bin`，`.pdm` 装到 `D:\.pmm\root`）
- `pmm install xxx -pc`   → 回到用户目录 `C:\Users\<你>\.pmm`
- `pmm install xxx -pe`   → 任意其它盘（`E:\.pmm\`）；也支持 `-p D`（空格分隔）写法

**记忆规则**：第一次用某个 `-p<盘>` 会被记住（写入 `~/.pmm/install-drive`），
之后的命令不写也会自动用那个盘；想换盘用新的 `-p<盘>` 即可。

> 注意：换盘会把配置/镜像源也挪到对应盘的 `.pmm`（如 `D:\.pmm\mirror.ini`）。
> 切换后如要装包，先在该盘目录放好镜像配置，或重新 `pmm mirror add ...`。

`--git` 对任意 git 仓库地址自动探测 API 形状（Gitea/Forgejo → GitLab → GitHub），
GitHub.com、GitLab.com、Gitea、Forgejo 及一切私有部署实例都支持。

## 下载校验（SHA-256 / SHA-1）

每次下载完成后自动计算并打印 SHA-256 与 SHA-1；若 release 资源旁有
`<资源名>.sha256` 或 `<资源名>.sha1` 伴随文件，会自动核对，不一致时拒绝安装
并删除缓存文件。registry 条目里若带 `sha256` 字段，也会作为独立校验来源。
SHA 实现为纯 C 内置（FIPS 180-4），无外部依赖。

## .pdm 包格式（模拟 dpkg 的 deb）

`pmm pack ./目录` 会把目录打包成一个 `.pdm`（一个 tar 归档，含
`control.tar.gz` / `data.tar.gz` / `sha256sums` 三个成员，与 deb 的
`control.tar`+`data.tar` 思路一致）：

- 目录里需有一个 `pdm-control` 文件，格式为 `KEY: value` 行：
  `Package` / `Version` / `Architecture` / `Description` / `Maintainer`
- 其余文件全部作为文件有效载荷，安装时解包到安装根目录（`~/.pmm/root`）
- `pmm install <pkg.pdm>` 安装，`pmm remove` 卸载
- 安装记录写入 `~/.pmm/installed/<包名>.info`，含文件清单，用于卸载

## apt 式镜像（mirror.ini / mirror.conf）

镜像配置文件位于 `~/.pmm/`（Windows 为 `%USERPROFILE%\.pmm\`），查找顺序
`mirror.ini` → `mirror.conf`。每个 `[名]` 是一个镜像，字段如下：

```ini
# ~/.pmm/mirror.ini
[main]
registry = https://pmm.parlz.com/mirror/packages
priority = 1

[sz]
registry = https://sz.pmm.parlz.com/mirror/packages
priority = 20
default = true

# 其它字段（可选）：
download = https://ghfast.example-mirror.dev      # 下载前缀（gh-proxy 风格，接到原始 URL 前）
api = https://api.mirror.example                  # git 主机 API 基地址（整体替换）
```

- `pmm install <包>` 会**按 priority 从小到大依次尝试**每个镜像的
  `{registry}/{包}.json`，第一个成功的镜像命中，否则回退到下一个（apt 的逻辑）。
- 下载文件时也会把镜像的 `download` 前缀按优先级排在原始 URL 之前逐个尝试。
- 每家的 `priority` 相同则按出现顺序；`pmm mirror use <名>` 可强制优先用某个镜像。

## 镜像仓库（mirror/ 静态目录）

`mirror/` 现在只做一件事：**静态包仓库**（无网页、无上传），把 `packages/` 目录用
任意静态文件服务器直接对外提供即可：

```sh
cd mirror
php -S 0.0.0.0:8080                   # 浏览页 + 静态下载（PHP，无 router 参数）
# 或 nginx/apache：DirectoryIndex index.php，.pdm 走静态文件
```

- 目录约定：`packages/<包>.json`（latest，含 `variants`）、
  `packages/<包>/<版本>-<平台>.pdm`（包文件）、`packages/<包>/<版本>-<平台>.json`（元数据）。
- 客户端把 `~/.pmm/mirror.ini` 指到该目录的 `packages`：
  ```ini
  [main]
  registry = http://<host>:8080/packages      # 生产 https://pmm.parlz.com/mirror/packages
  priority = 1
  ```
  然后 `pmm install nodejs`（自动识平台）即可从这个静态仓库下载安装。
- `mirror.ini` 也可从此目录按需生成（`registry` 指向 `/packages`）。

`pmm install <包>` 装 latest，也支持 **Python/pip 风格的版本限定**：

```sh
pmm install nodejs                 # latest
pmm install nodejs==24.20.0        # 精确版本
pmm install nodejs>=24,<25         # 版本区间（选出范围内最高）
pmm install nodejs<=24.20.0        # <= / >= / < / > / != , 逗号组合
pmm install "nodejs>=24,<25"       # 带空格/组合时加引号
```

## Release 资源与平台对应

| 系统 | 优先匹配的扩展名 |
|---|---|
| windows | `.exe` `.msi` `.zip` `.7z` |
| linux | `.deb` `.rpm` `.appimage` `.tar.gz` `.tgz` `.tar.xz` `.tar.bz2` `.pkg.tar.zst` |
| macos | `.dmg` `.pkg` `.app.zip` `.tar.gz` `.tgz` |

校验文件 / 源码包（`.sha256`、`checksum*`、`-src` 等）会被自动跳过。

## 主机支持

- **GitHub**：`https://api.github.com/repos/{repo}/releases/latest`
- **GitLab**：`https://gitlab.com/projects/{repo}/releases/permalink/latest`
- **Gitea / Forgejo**（含私人部署）：`{origin}/api/v1/repos/{repo}/releases/latest`

镜像源会整体替换 API 基地址，加速代理和私有实例都走同一条路。

## 安装行为（下载后自动安装）

下载完成并通过校验后，pmm 会自动执行对应平台的安装动作，无需再手动操作：

- `.deb` → `sudo dpkg -i`；`.rpm` → `sudo rpm -Uvh`（Linux）
- `.msi` → `msiexec /i /qn`（Windows，静默安装）
- `.exe` → 自动运行安装器（Windows，带常见静默旗标 `/S /VERYSILENT /quiet --silent`）
- `.dmg` → `hdiutil attach` + `sudo cp` 复制 `.app` 到 `/Applications`；`.pkg` → `sudo installer`（macOS）
- `.tar.*` / `.zip` / `.7z` → 解压到安装目录（`~/.pmm/bin`）
- `.pdm` → 通过内置 .pdm 管理器解包到 `~/.pmm/root`；`.appimage` → 加执行权限后放入安装目录

> Windows 下 pmm 通过系统 `cmd.exe` 执行命令，因此 `.exe` 用 cmd 兼容的单条静默命令。

## 已安装软件登记到注册表（Installed Apps）

安装 `.pdm` 包后，pmm 会模仿 **MSI/NSIS 安装器** 的做法，把软件写入 **每用户
注册表卸载项** `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\<包名>`，
于是它会出现在 Windows「应用 / Installed Apps」里，带：

- `DisplayName` / `DisplayVersion` / `Publisher`（取自控制信息）
- `DisplayIcon`（包内找到的第一个 `.exe`）
- `UninstallString` → `"<pmm.exe>" remove <包名>`，让「卸载」按钮真正调用我们
- `InstallLocation`、`EstimatedSize`、`InstallDate`、`NoModify`、`NoRepair`

`pmm remove <包>`（或控制面板"卸载"按钮）会删除文件并清理该注册表项。
（每用户 HKCU 写入，无需管理员权限，重启终端/刷新设置可见。）

## 自动加入 PATH

每次成功安装后，pmm 会把 PMM 目录 **自动加入用户 PATH**（幂等，不重复）：

- 始终加入 `~/.pmm/bin` 与 `~/.pmm/root`；
- 还会扫描 `~/.pmm/root/` 下含有可执行文件的子目录并加入，例如安装 Node.js 后
  会自动加入 `~/.pmm/root/nodejs`；对 `pkg/cmd` 布局（如 Git/MinGit 的
  `git/cmd/git.exe`）会识别到第二层并加入 `~/.pmm/root/git/cmd`。
- Windows 通过用户注册表 PATH 持久化（`setx`），新开终端即生效；Linux/macOS
  写 `~/.bashrc`（重新加载 shell 生效）。

## 现成的 .pdm 包（dist/）

- `nodejs-24.20.0.pdm` — Node.js 24.20.0（Windows，源自官方 MSI）
- `nodejs-linux-24.20.0.pdm` — Node.js 24.20.0（Linux x64，源自官方 .tar.xz）
- `git-2.55.0.5.pdm` — Git for Windows 2.55.0.5（MinGit 官方发行版）
- `pureftpd-1.0.51.pdm` — **Pure-FTPd 1.0.51（Linux）**：源码分发，装到 Linux 的 `~/.pmm/root`
  后需 `./configure && make && sudo make install` 编译使用（官方只发布源码）。

## 自动识别平台

镜像站把同一软件做成**多平台变体**（`variants` 数组，每项带 `os`）。客户端安装时
**自动按当前系统选平台**：

```sh
pmm install nodejs        # Windows 上装 node.exe 版，Linux 上自动装 ELF 版
pmm install nodejs
```

- Windows 机 `install nodejs` → 拉取 `os=windows` 变体（`packages/nodejs/24.20.0-windows.pdm`）
- Linux 机 `install nodejs` → 拉取 `os=linux` 变体（`packages/nodejs/24.20.0-linux.pdm`）
- 版本语义仍支持 `==` / `>=` / `<=` / `,` 组合，且在**匹配当前平台**的版本里选最高。

## 跨环境可靠性

`.pdm`/`.deb` 等 tar 相关安装动作先切换到 PMM 目录再用**相对路径**执行，避免
git-bash 的 MSYS GNU tar（认 `/c/...`）与 Windows 自带的 bsdtar（认 `C:\...`）
对盘符路径解析不一致的问题。

## registry 格式（pmm install xxx）

每个镜像的 `{registry}/{包名}.json` 描述一个包；`url` + `file` 指向发布文件，
`sha256`（可选）作为该文件的独立校验来源：

```json
{ "version": "1.2.3", "url": "https://.../foo-1.2.3-win64.exe",
  "file": "foo-1.2.3-win64.exe", "sha256": "..." }
```

## 目录结构

```
src/main.c       命令入口（pmm）
src/json.c/.h    内置 JSON 解析器
src/ini.c/.h     INI/.conf 解析器
src/http.c/.h    系统 curl 封装
src/repo.c/.h    仓库适配（GitHub/GitLab/Gitea/Forgejo + 通用 URL 自动探测）
src/sha256.c/.h  SHA-256（FIPS 180-4）
src/sha1.c/.h    SHA-1（FIPS 180-4）
src/mirrors.c/.h apt 式镜像列表（priority 排序 + 下载回退）
src/pdm.c/.h     .pdm 打包/安装/卸载（模拟 deb）
src/install.c/.h 下载、校验与各平台安装
mirror/           镜像站（Node.js 网页版）
examples/        配置文件示例
```
