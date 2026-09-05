# 源码

PMM 是开源项目（GPL-3.0）。

- **GitHub 仓库**：[JGZYES/ParlzPackageManger](https://github.com/JGZYES/ParlzPackageManger)
- **GitHub Releases**：[releases](https://github.com/JGZYES/ParlzPackageManger/releases)
- **提交 Issue / PR**：[issues](https://github.com/JGZYES/ParlzPackageManger/issues)

## 构建

需要 C11 编译器 + 静态链接环境：

```bash
# Linux
gcc -O2 -Wall -static -std=c11 -o pmm \
  src/main.c src/json.c src/ini.c src/pmm.c src/http.c src/repo.c \
  src/install.c src/sha256.c src/sha1.c src/mirrors.c src/pdm.c src/out.c src/i18n.c
```

## 文档规范

界面文案、`.pjson` 语言包键名、版本 banner 等遵循 [STANDARD.md](https://github.com/JGZYES/ParlzPackageManger/blob/main/docs/STANDARD.md)。
