# 核心特性

- **apt 式多镜像源**：按优先级回退，`mirror.ini` 可配多个 registry。
- **Git Release 安装**：直接装 GitHub / GitLab / Gitea / Forgejo 的最新 Release。
- **`.pdm` 打包格式**：deb 风格的内置包管理器，跨平台。
- **依赖解析**：安装时自动解析 `Depends`。
- **并发下载 + 断点续传**：大文件分片并行、断点续传。
- **哈希校验**：下载后 sha256 / sha1 校验，拒绝校验失败。
- **本地包安装**：`.deb` / `.rpm` / `.apk` / `.msi` / `.zip` / `.pkg.tar.zst` 都能装。
- **搜索与信息**：`pmm search` / `pmm info`。
- **缓存管理**：`pmm cache clean`。
- **彩色输出 + 语言包**：`[PMM]:[LEVEL]` 结构化输出，`.pjson` 多语言。
- **i18n**：`pmm setting lang <locale>` 切换语言，社区可贡献。

详见 [命令参考](index.php?page=cli) 与 [翻译](index.php?page=translate)。
