# 下载

PMM 提供多种安装包。官网镜像站保留二进制注册表（`mirror/`），也可直接从 GitHub Release 获取。

## 一键安装（推荐）

```bash
curl -sSL https://pmm.parlz.com/install.sh | bash
```

> 说明：`install.sh` 现在由 GitHub Release 提供；若你手动部署，可把仓库里的 `install.sh` 放到站点根目录，或直接用下方的 GitHub 链接。

## 各平台产物（GitHub Release）

- [pmm-0.3.6-windows-amd64.zip](https://github.com/JGZYES/ParlzPackageManger/releases/latest) — Windows 免安装
- [pmm_0.3.6_amd64.deb](https://github.com/JGZYES/ParlzPackageManger/releases/latest) — Debian/Ubuntu
- [pmm-0.3.6.x86_64.rpm](https://github.com/JGZYES/ParlzPackageManger/releases/latest) — CentOS/RHEL
- [pmm-0.3.6-linux-amd64.pdm](https://github.com/JGZYES/ParlzPackageManger/releases/latest) — `.pdm` 包
- [pmm-0.3.6.apk](https://github.com/JGZYES/ParlzPackageManger/releases/latest) — Alpine
- [pmm-0.3.6.pkg.tar.zst](https://github.com/JGZYES/ParlzPackageManger/releases/latest) — Arch

## 用 pmm 安装（从镜像注册表）

mirror 注册表保留在 `web/mirror/`，pmm 客户端可直接安装：

```bash
pmm install pmm==0.3.6          # 升级 pmm 自身
pmm install nodejs              # 装一个软件包
```

## 镜像源

- **深圳（默认）**：`https://sz.pmm.parlz.com/mirror/packages`
- **香港（备份）**：`https://pmm.parlz.com/mirror/packages`

全部历史版本的 `.pdm` 都在镜像 `mirror/packages/pmm/` 下。详见 [镜像源](index.php?page=mirror)。
