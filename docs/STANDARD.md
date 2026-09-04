# PMM 统一标准 (STANDARD.md)

> Parlz Package Manager (PMM)。核心理念:**敲即得，装即跑** —— 在命令行上安装软件。
> 本文件是 PMM 的**文案、输出、版本、CHANGELOG、语言包**的统一规范，所有改动须遵循。

---

## 1. 版本 Banner 格式

所有版本信息一律使用：

```
PMM <版本号> (<架构>)
```

示例：

```
PMM 0.3.5 (amd64)
PMM 0.3.5 (aarch64)
PMM 0.1.0 (x86)
```

- 大写的 `PMM` + 空格 + 版本号 + 空格 + `(架构)`。
- 架构由 `pmm_detect_arch()` 提供（`amd64` / `aarch64` / `x86` / `any`）。
- `pmm version` / `pmm -v` / `--version` 输出这一行，**不带** OS 名，不带方括号。
- Bash/脚本中可用 `pmm version` 解析版本。

## 2. 命令文案措辞表

命令描述统一用**动词开头**的祈使句；选项描述统一用 `动词 + 宾语`。中文 / 英文措辞见下表（也是 `.pjson` 的标准译文）：

| 命令 | 中文 | English |
|---|---|---|
| `list` | 根据名称列出软件包 | list packages by name |
| `info` | 根据名称列出软件包的详细信息 | show detailed info for a package |
| `search` | 根据关键词搜索软件包 | search packages by keyword |
| `install` | 根据名称安装指定软件包 | install a package by name |
| `remove` | 根据名称卸载指定软件包 | uninstall a package by name |
| `update` | 刷新镜像源索引（apt 风格） | refresh the registry index (apt-style) |
| `upgrade` | 升级已安装的软件包 | upgrade all installed packages |
| `mirror` | 列出 / 管理镜像源 | list / manage mirrors |
| `self-update` | 更新 PMM 工具本身 | update the pmm tool itself |
| `version` / `help` | 查看版本 / 帮助 | show version / help |

> 命令正文为**小写**；`help` 输出里每条命令后跟该措辞。

## 3. Help 排版规范

`pmm help` 输出遵循固定排版：

```
PMM 0.3.5 (amd64)

usage:
  pmm <command> <args...>

commands:
  list  - 根据名称列出软件包
  info  - 根据名称列出软件包的详细信息
  ...

options:
  -p<drive>   install under <DRIVE>:\.pmm
  --no-color  omit ANSI colours on a terminal
```

规则：

- **第一行**必须是 banner（见 §1），后接空行。
- `commands:` 段落：每行 `  <command>  -  <描述>`，命令与描述间**对齐**（列宽 ≥ 22）。
- `options:` 段落：每行 `  <flag>  <解释>`，flag 与解释间对齐。
- 描述文本可含中英文（由语言包决定），但**排版（空格/对齐）不随语言变化**。
- 帮助靠**语言包**渲染；无语言包时回退内置中文。

## 4. CHANGELOG 格式

`CHANGELOG/<版本>.md`，每版本一个文件，模板：

```md
# PMM v<版本>

## 新增
- <功能>

## 修复
- <bug>

## 变更
- <行为变化>

## 说明
- <注意事项>
```

规则：

- 文件名为 `CHANGELOG/<版本>.md`（如 `CHANGELOG/0.3.5.md`）。
- 章节顺序固定：`## 新增` → `## 修复` → `## 变更` → `## 说明`；无内容的章节可省略。
- 每条款以 `- ` 开头，动词开头，**不含**句号。
- 版本号与 `PMM_VERSION` 一致。

## 5. `.pjson` 语言包键命名规范

语言包为扁平 JSON，一个文件一种语言，后缀 `.pjson`：

```
web/mirror/lang/<locale>.pjson
```

`<locale>` 采用 BCP-47：`zh-CN` / `en-US` / `zh-TW` / `ja-JP` …

```
{
  "help.banner": "PMM {ver} ({arch})",
  "cmd.list": "list",
  "cmd.info": "info",
  "desc.list": "根据名称列出软件包",
  "msg.installed": "已安装 {name}",
  "msg.err.notfound": "未找到软件包 {name}"
}
```

键命名规则：**`<区域>.<语义>`**，用`.`分隔。

| 前缀 | 含义 |
|---|---|
| `help.` | 帮助 / banner |
| `cmd.` | 命令名本身 |
| `desc.` | 命令描述 |
| `msg.` | 运行期消息 |
| `msg.err.` | 错误消息 |
| `msg.warn.` | 警告消息 |
| `opt.` | 选项说明 |
| `cfg.` | 配置项名 |

占位符用 `{name}`、`{ver}`、`{arch}` 等（花括号 + 小写字段名），渲染时替换。
键**不得**含空格 / 大写，语义词用小写连字符（如 `msg.err.not-found`）。

## 6. 提交 / PR 约定

- 每个新功能或修复：`CHANGELOG/` 新增对应版本文件（仅当进入发布版本时）。
- 改 `src/` 下任何用户可见字符串时，**同步更新** `web/mirror/lang/zh-CN.pjson` 与 `en-US.pjson` 对应键。
- 新增 `.pjson` 键必须同时出现在 `zh-CN`、`en-US`（中文为基准，英文为对齐）。
- 提交信息：`type(scope): description`，`type` ∈ `feature|fix|docs|i18n|ci|refactor`。

---

## 本项目的落地版本

- `src/main.c` 的 `print_help()` 与 `pmm version` 输出严格遵循 §1、§3。
- 翻译字符串通过 `pmm_tr(key)` / `pmm_tr_fmt(key, ...)` 取用，见 `src/i18n.h`。
- 版本常量：`PMM_VERSION`（构建时 `-DPMM_VERSION="..."` 注入），默认 `0.1.0`。
