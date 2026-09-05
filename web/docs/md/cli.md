# 命令参考

## 安装

```bash
pmm install <pkg>                     # 从注册表安装
pmm install <pkg> [pkg2 ...]          # 装多个
pmm install <file.deb|.rpm|.apk|.pdm> # 装本地包
pmm install -dpkg <x.deb>             # 强制按 deb 装
pmm install -rpm <x.rpm>              # 强制按 rpm 装
pmm install --git <repo>              # 从 git Release 装
pmm install --force <pkg>             # 强制重装
```

## 卸载 / 查询

```bash
pmm remove <pkg>
pmm list                       # 已安装
pmm list --upgradable          # 可升级的
pmm search <keyword>           # 搜索
pmm info <package|file.pdm>    # 详情
pmm verify <file>              # sha256/sha1
```

## 镜像 / 更新

```bash
pmm setting mirror list
pmm setting mirror add <n> <api>
pmm setting mirror use <n>
pmm setting mirror remove <n>
pmm setting mirror check             # 探测各镜像源可达性
pmm update                     # 刷新镜像索引
pmm upgrade [--yes]            # 升级所有已装包
pmm self-update                # 更新 pmm 自身
```

## 配置

```bash
pmm setting lang -l              # 列语言包
pmm setting lang <locale>        # 切换语言
pmm setting set <key> <value>    # 写 pmm.conf（如 mirror / proxy）
pmm setting get                  # 读 pmm.conf
pmm setting mirror <name>        # 设当前镜像
pmm cache clean                  # 清缓存
```

## 全局选项

```bash
--no-color   # 禁用 ANSI 颜色
-q, --quiet  # 只显示错误/警告
--verbose    # 更多细节
-p<drive>    # 指定安装盘符（Windows）
```
