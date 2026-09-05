#!/bin/sh
# ParlzPackageManager (PMM) — Linux/macOS one-line installer.
#
# 用法：
#   curl -sSL -o install.sh https://pmm.parlz.com/install.sh && bash install.sh
#   或：curl -sSL https://pmm.parlz.com/install.sh | bash
#
# 下载官方 release 二进制并安装到 /usr/local/bin/pmm（root）或 ~/.local/bin/pmm。
set -e
VER="0.3.7"
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

# ---- 安装完成:提示如何让当前 shell 立即用上新 pmm ----
DEST_DIR="$(dirname "$DEST")"
case ":$PATH:" in
  *":$DEST_DIR:"*) ;;
  *) echo "pmm: 请将 $DEST_DIR 加入 PATH（或重新登录 shell）" ;;
esac

# 旧 shell 可能把 pmm 命中在已不存在的旧路径（如 ~/.pmm/root/bin/pmm），导致
# 报 "No such file or directory"。bash 用 `hash -r`、zsh 用 `rehash`
# 重新按 PATH 解析。子脚本无法清父 shell 的命中缓存，这里尽力清一次并给出提示。
hash -r 2>/dev/null || true

echo "pmm: 安装完成 -> $DEST"
echo "pmm: 若当前 shell 里 pmm 仍不可用/报错，请先执行:  hash -r    (zsh 用: rehash)"
"$DEST" -v 2>/dev/null || true
