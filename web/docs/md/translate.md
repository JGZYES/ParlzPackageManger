# 翻译 / 语言包

PMM 的界面文案由 `.pjson` 语言包驱动。默认内置 **zh-CN** 与 **en-US**，社区可以提交新的语言包或修订译文。

## 安装语言包

```bash
pmm setting lang -l               # 列已有语言包
pmm setting lang ja-JP            # 安装并切换
```

## 如何贡献一门语言

1. 从镜像下载一个语言包作模板（如 `zh-CN.pjson`）。
2. 把键名保持不变、只改值，另存为你的语言，如 `ja-JP.pjson`、`zh-TW.pjson`。
3. 将文件发到 **`luoriguodu@qq.com`**，或在 **GitHub 提 Issue**。
4. 通过后进入镜像 `web/mirror/lang/`，用户即可 `pmm setting lang <locale>` 安装。

## 语言包格式

```json
{
  "help.banner": "PMM %s (%s)",
  "cmd.install": "install",
  "desc.install": "根据名称安装指定软件包",
  "msg.installed": "已安装 %s"
}
```

键命名：`<区域>.<语义>`（如 `msg.err.not-found`），占位符用 `%s` / `%d`。

详见 [STANDARD.md](https://github.com/JGZYES/ParlzPackageManger/blob/main/docs/STANDARD.md)。
