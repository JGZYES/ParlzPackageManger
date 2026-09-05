# 安装

所有安装包在官网 `download/` 目录。

## Linux / macOS 一键

```bash
curl -sSL https://pmm.parlz.com/download/install.sh | bash
```

## Windows

下载并解压 zip，将 `pmm.exe` 加入 PATH：

```bash
# PowerShell
Invoke-WebRequest https://pmm.parlz.com/download/pmm-0.3.8-windows-amd64.zip -OutFile pmm.zip
Expand-Archive pmm.zip -DestinationPath pmm
```

> 然后将 `pmm\pmm.exe` 所在目录加入 PATH。

## Debian / Ubuntu（.deb）

```bash
curl -L -o pmm.deb https://pmm.parlz.com/download/pmm_0.3.8_amd64.deb
sudo apt install ./pmm.deb
```

## CentOS / RHEL（.rpm）

```bash
curl -L -o pmm.rpm https://pmm.parlz.com/download/pmm-0.3.8.x86_64.rpm
sudo rpm -Uvh pmm.rpm
```

## 用 pmm 安装 pmm 自身

```bash
pmm install pmm==0.3.8
```

## 装好后

```bash
pmm search nodejs
pmm install nodejs
pmm list
pmm remove nodejs
```

详见 [命令参考](index.php?page=cli) 与 [下载](index.php?page=download)。
