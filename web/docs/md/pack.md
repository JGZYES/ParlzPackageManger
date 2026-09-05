# 打包 .pdm 包

`.pdm` 是 PMM 的包格式（deb 风格），用 `pmm pack` 把一个目录打包成 `.pdm`，供 `pmm install` 安装、并发布到镜像注册表。

## 目录结构

一个可打包的目录需包含 **`pdm-control`** 控制文件，其余任意文件作为包内容：

```
myapp/
├── pdm-control        # 必填：包元数据
└── bin/
    └── myapp          # 可执行文件（或任意要安装的文件）
```

> `bin/` 里的内容安装时会被“摊平”到目标目录；其它目录树原样保留。

## pdm-control 字段

```text
Package: myapp              # 必填，包名
Version: 1.0.0              # 必填，版本号
Architecture: linux         # windows | linux | macos | all
Description: My App         # 描述
Maintainer: You <you@x.com> # 维护者
Depends: libc               # 可选，依赖（安装时自动解析）
License: MIT                # 可选
Homepage: https://x.com     # 可选
```

- **Package / Version 必须**；其余可省略。
- `Architecture` 通常填当前平台（`windows` / `linux` / `macos`）。

## 打包

```bash
pmm pack ./myapp myapp-1.0.0.pdm
```

不指定输出文件名时，默认生成 `<Package>_<Version>.pdm`：

```bash
pmm pack ./myapp            # -> myapp_1.0.0.pdm
```

## 安装/测试打好的包

```bash
# 本地安装
pmm install ./myapp_1.0.0.pdm

# 查看包信息
pmm info ./myapp_1.0.0.pdm

# 安装到指定目录
pmm install -p /opt/myapp ./myapp_1.0.0.pdm
```

## 发布到镜像

包体在 `web/mirror/packages/<name>/`，并往 `web/mirror/packages/<name>.json` 增加一个变体：

```json
{
  "name": "myapp",
  "version": "1.0.0",
  "os": "linux",
  "arch": "amd64",
  "variants": [
    {
      "version": "1.0.0",
      "os": "linux",
      "arch": "amd64",
      "file": "1.0.0-linux-amd64.pdm",
      "url": "https://pmm.parlz.com/mirror/packages/myapp/1.0.0-linux-amd64.pdm",
      "sha256": "<包的 sha256>",
      "description": "My App"
    }
  ]
}
```

之后即可：

```bash
pmm install myapp
```

## 校验

打包后建议校验完整性：

```bash
pmm verify ./myapp_1.0.0.pdm
```

详见 [镜像源](index.php?page=mirror) 与 [命令参考](index.php?page=cli)。
