# ppdm — 打包与发布工具

`ppdm`(ParlzPackageDevManager)是 PMM 生态里的**开发者工具**：把软件打包成 `.pdm` 并发布到镜像注册表。
它与 `pmm`（安装/使用包）分开，负责“做包、发包”。

- 版本：独立版本号（当前 `0.0.1`），不受 PMM 主版本影响。
- 服务端：`https://pmm.parlz.com/ppdm`（PHP，对应仓库 `web/ppdm/`）。
- 配置：`~/.ppdm/config`（Linux）或 `D:\.ppdm\config`（Windows）。

## 安装

### 一键安装（推荐）

```bash
curl -sSL https://pmm.parlz.com/download/install-ppdm.sh | bash
hash -r
ppdm -v        # ppdm 0.0.1
```

脚本会自动下载最新 `ppdm` 装到 `~/.ppdm/bin/ppdm`（与 `ppdm update` 同一路径），并把 `~/.ppdm/bin` 加进 PATH。

### 手动下载

```bash
# Linux amd64
curl -L -o ~/.local/bin/ppdm \
  https://github.com/JGZYES/ParlzPackageManger/releases/download/v0.5.5/ppdm
chmod +x ~/.local/bin/ppdm

# 或自更新到 ~/.ppdm/bin/ppdm
ppdm update
```

验证：

```bash
ppdm -v       # ppdm 0.0.1
ppdm help
```

## 命令总览

| 命令 | 作用 |
|------|------|
| `ppdm register <email> <password>` | 注册账号（本地算术人机验证，答对才创建） |
| `ppdm login <email> <password>` | 登录，保存 token |
| `ppdm logout` | 撤销并清除 token |
| `ppdm whoami` | 显示当前邮箱 + 服务器 |
| `ppdm pack <dir> [out]` | 把 `dir` 打包成 `.pdm`（`dir` 内需有 `pdm-control`） |
| `ppdm update` | 更新 ppdm 自身 |
| `ppdm ./xxx.pdm` | 发布包（自动生成 json） |
| `ppdm help` | 帮助 |

## 1. 注册 / 登录

```bash
ppdm register you@mail.com 你的密码     # 出一道算术题，答对即注册成功
ppdm login    you@mail.com 你的密码     # 保存 token，之后可发布
```

> 人机验证在**本地**完成（一道中等难度算术题），不依赖邮件/服务器。

## 2. 打包成 .pdm

在任意目录准备源文件 + `pdm-control`（`Package/Version/Architecture/Description/Maintainer` 字段）：

```text
myapp/
  pdm-control          # 见下
  bin/myapp            # 可执行文件、配置等（会被装进 ~/.pmm/root 下）
```

`pdm-control` 示例：

```ini
Package: myapp
Version: 1.0.0
Architecture: linux
Description: My app
Maintainer: you@mail.com
```

打包：

```bash
ppdm pack ./myapp            # 生成 myapp_1.0.0.pdm
ppdm pack ./myapp out/my.pdm # 指定输出名
```

## 3. 发布（自动生成 json）

```bash
ppdm ./myapp_1.0.0.pdm
```

服务端会：
- 校验登录 token；
- 计算 sha256；
- 写入 `web/mirror/dists/<pkg>.json`（variants）+ `dists/<pkg>/<version>.json`；
- 把 `.pdm` 放到 `web/mirror/files/<首字母>/<pkg>/`；
- 追加到 `dists/packages.json`。

**重复与权限**：
- 同 `name` 已存在且是**你创建的** → 可以继续上传**新版本**；
- 同版本重复上传 → 409；
- 别人改你的包 → 409（阻止越权）。

## 4. 用 pmm 安装刚发布的包

```bash
# pmm 的镜像 base 需指向 dists：~/.pmm/mirror.ini
#   registry = https://pmm.parlz.com/mirror/dists
pmm update
pmm install myapp
```

## 5. 更新 ppdm

```bash
ppdm update      # 下载最新 ppdm 二进制到 ~/.ppdm/bin/ppdm
```

## 常见问题

- **发布失败：`package 'xx' already exists (owner: ...)`** —— 该包不是你发布的，或版本重复。
- **本地算术题**每次都随机，答错可重跑。
- **服务端地址**默认 `https://pmm.parlz.com/ppdm`；如需改：在 `~/.ppdm/config` 写 `server = <url>`。
- **安装后 `pmm install <pkg>` 找不到** —— 确认 `~/.pmm/mirror.ini` 的 registry 是 `.../mirror/dists`（不是旧的 `/packages`）。
