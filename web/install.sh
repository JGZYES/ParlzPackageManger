#!/bin/sh
# ParlzPackageManager (PMM) — Linux/macOS one-line installer.
#
# 用法：
#   curl -sSL -o install.sh https://pmm.parlz.com/install.sh && bash install.sh
#   或：curl -sSL https://pmm.parlz.com/install.sh | bash
#
# 下载官方 release 二进制并安装到 /usr/local/bin/pmm（root）或 ~/.local/bin/pmm。
set -e
VER="0.5.0"
REPO="JGZYES/ParlzPackageManger"

# ---- 检测平台 ----
OS="$(uname -s)"; ARCH="$(uname -m)"
case "$OS" in
  Linux)  os=linux ;;
  Darwin) os=macos ;;
  *) echo "pmm: 不支持的平台 $OS"; exit 1 ;;
esac
case "$ARCH" in
  x86_64|amd64) arch=amd64 ;;
  aarch64|arm64) arch=arm64 ;;
  *) echo "pmm: 不支持的架构 $ARCH"; exit 1 ;;
esac

# ---- 下载 pmm（按架构选 release 资产名）----
case "$arch" in
  amd64) ASSET="pmm" ;;
  arm64) ASSET="pmm-aarch64" ;;
esac
URL="https://github.com/$REPO/releases/download/v$VER/$ASSET"
echo "pmm: 下载 v$VER ($os/$arch) ..."
TMP="$(mktemp)"
# --progress-bar shows a live download progress line (tty only)
if ! curl -fL --progress-bar --max-time 300 -o "$TMP" "$URL"; then
  rm -f "$TMP"; echo "pmm: 下载失败（$URL）"; exit 1
fi
echo ""
chmod +x "$TMP"

# ---- 安装 ----
if [ "$(id -u)" = "0" ]; then
  DEST="/usr/local/bin/pmm"
else
  DEST="${HOME}/.local/bin/pmm"
fi
mkdir -p "$(dirname "$DEST")"
cp "$TMP" "$DEST" && chmod +x "$DEST" && rm -f "$TMP"

# ---- 安装完成:检测当前 shell 会解析到哪个 pmm，并给出精确指引 ----
DEST_DIR="$(dirname "$DEST")"
case ":$PATH:" in
  *":$DEST_DIR:"*) ;;
  *) echo "pmm: 请将 $DEST_DIR 加入 PATH（或重新登录 shell）" ;;
esac

# bash 可能仍把 pmm 命中在旧路径（如已删除的 ~/.pmm/root/bin/pmm），或命中到
# WSL 里 >/mnt/ 下 Windows 侧安装的 pmm。子脚本清不了父 shell 缓存，但 command -v
# 能报出“重新解析 PATH 后”会命中哪个 pmm，据此给出精确的三步操作。
hash -r 2>/dev/null || true
FOUND="$(command -v pmm 2>/dev/null || true)"

echo ""
echo "pmm: 安装完成 -> $DEST"
"$DEST" -v 2>/dev/null || true

if [ -n "$FOUND" ] && [ "$FOUND" != "$DEST" ]; then
  echo ""
  echo "pmm: 注意: 当前 shell 里的 'pmm' 仍会命中: $FOUND"
  echo "pmm:      (而不是刚安装的 $DEST)"
  case "$FOUND" in
    /mnt/*) echo "pmm:      它在 /mnt/ 下，通常是 Windows 侧安装的 pmm 经由 WSL 的 PATH 被带进来。" ;;
  esac
  echo "pmm: 请在当前终端执行下面三条，让 WSL 优先用刚装的 Linux 版："
  echo "pmm:"
  echo "pmm:   export PATH=\"$DEST_DIR:\$PATH\""
  echo "pmm:   hash -r"
  echo "pmm:   which pmm && pmm -v"
  echo "pmm:"
  echo "pmm: 永久生效(写入 ~/.bashrc 后 source)："
  echo "pmm:   echo 'export PATH=\"$DEST_DIR:\$PATH\"' >> ~/.bashrc && source ~/.bashrc"
fi
