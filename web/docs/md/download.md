# 下载

所有安装包统一放在官网 `download/` 目录,也可从 GitHub Release 获取。

## 一键安装（推荐）

```bash
curl -sSL https://pmm.parlz.com/download/install.sh | bash
```

## 各平台产物（`/download/`）

- [pmm-0.5.0-windows-amd64.zip](/download/pmm-0.5.0-windows-amd64.zip) — Windows 免安装
- [pmm_0.5.0_amd64.deb](/download/pmm_0.5.0_amd64.deb) — Debian/Ubuntu
- [pmm-0.5.0.x86_64.rpm](/download/pmm-0.5.0.x86_64.rpm) — CentOS/RHEL
- [pmm-0.5.0.apk](/download/pmm-0.5.0.apk) — Alpine
- [pmm-0.5.0.pkg.tar.zst](/download/pmm-0.5.0.pkg.tar.zst) — Arch
- [pmm](/download/pmm) — Linux 二进制
- [pmm.exe](/download/pmm.exe) — Windows 二进制
- [install.sh](/download/install.sh) — 一键安装脚本

> `.pdm` 包走镜像注册表:`pmm install pmm==0.5.0`(见 [镜像源](index.php?page=mirror))。

## 用 pmm 安装（从镜像注册表）

```bash
pmm install pmm==0.5.0
pmm install nodejs
```

## 镜像源

- **深圳（默认）**：`https://sz.pmm.parlz.com/mirror/packages`
- **香港（备份）**：`https://pmm.parlz.com/mirror/packages`

历史版本的 `.pdm` 都在镜像 `mirror/packages/pmm/` 下。详见 [镜像源](index.php?page=mirror)。
