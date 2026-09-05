# 镜像源

PMM 采用 apt 式多镜像源，按优先级回退。配置在 `~/.pmm/mirror.ini`。

## 默认镜像

- **深圳（默认）**：`https://sz.pmm.parlz.com/mirror/dists`
- **香港（备份）**：`https://pmm.parlz.com/mirror/dists`

## mirror.ini 格式

```ini
[sz]
registry = https://sz.pmm.parlz.com/mirror/dists
priority = 1
default = true

[main]
registry = https://pmm.parlz.com/mirror/dists
priority = 20
```

## 管理镜像

```bash
pmm setting mirror list          # 列出
pmm setting mirror add <n> <api> # 添加
pmm setting mirror use <n>       # 设为当前
pmm setting mirror remove <n>    # 移除
pmm setting mirror check         # 探测每个源的可达性/优先级
```

## 服务状态

首页有深圳/香港双镜像的实时探测：
- 注册表 `mirror/dists/pmm.json`
- `.pdm` 下载 `mirror/files/p/pmm/<ver>-linux-amd64.pdm`
- 镜像目录浏览 `mirror/`

详见 [下载](index.php?page=download)。
