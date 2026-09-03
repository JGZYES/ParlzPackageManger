#!/bin/sh
# ParlzPackageManager (PMM) — Linux/macOS one-line installer.
#
# 用法：
#   curl -sSL -o install.sh https://pmm.parlz.com/install.sh && bash install.sh
#   或：curl -sSL https://pmm.parlz.com/install.sh | bash
#
# 下载官方 release 二进制并安装到 /usr/local/bin/pmm（root）或 ~/.local/bin/pmm。
set -e
VER="0.3.3"
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

# ---- 下载 pmm（release 资产名为 pmm）----
URL="https://github.com/$REPO/releases/download/v$VER/pmm"
if [ "$arch" != "amd64" ]; then
  echo "pmm: 注意：官方发布目前只提供 amd64 资产，$os/$arch 可能 404"
fi
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

# ---- PATH 提示 ----
case ":$PATH:" in
  *":$(dirname "$DEST"):"*) ;;
  *) echo "pmm: 请将 $(dirname "$DEST") 加入 PATH（或重新登录 shell）" ;;
esac

echo "pmm: 安装完成 -> $DEST"
"$DEST" -v 2>/dev/null || true
