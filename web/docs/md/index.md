# 快速开始

**PMM（ParlzPackageManager）** 是一个用 C 语言从零编写的跨平台包管理器，理念：**敲即得，装即跑**。

支持 Windows / Linux / macOS。像 apt 一样多镜像源，也能直接装 GitHub / GitLab / Gitea / Forgejo 的 Release。

## 一分钟装好

```bash
curl -sSL https://pmm.parlz.com/download/install.sh | bash
```

装好后：

```bash
pmm --version      # 查看版本
pmm install nodejs # 安装一个软件包
pmm list           # 已安装的软件包
```

## 常用命令一览

```bash
pmm install <pkg>        安装
pmm remove <pkg>         卸载
pmm search <kw>          搜索
pmm info <pkg>           详情
pmm update               刷新镜像索引
pmm upgrade              升级所有已装包
pmm mirror list          查看镜像源
pmm setting lang <xx>    切换语言包
```

详见 [命令参考](index.php?page=cli)。
