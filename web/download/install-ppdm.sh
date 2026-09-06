#!/bin/sh
# ppdm (ParlzPackageDevManager) — Linux/macOS one-line installer.
#
# 用法：
#   curl -sSL https://pmm.parlz.com/download/install-ppdm.sh | bash
#   或：curl -sSL -o install-ppdm.sh https://pmm.parlz.com/install-ppdm.sh && bash install-ppdm.sh
#
# 下载官方 release 的 ppdm 二进制并安装到 ~/.ppdm/bin/ppdm（与 `ppdm update` 同一路径），
# 并把 ~/.ppdm/bin prepend 到 PATH。
set -e
REPO="JGZYES/ParlzPackageManger"

# ---- 检测平台 ----
OS="$(uname -s)"; ARCH="$(uname -m)"
case "$OS" in
  Linux)  os=linux ;;
  Darwin) os=macos ;;
  *) echo "ppdm: 不支持的平台 $OS"; exit 1 ;;
esac
case "$ARCH" in
  x86_64|amd64)   arch=amd64;   ASSET="ppdm" ;;
  aarch64|arm64)  arch=arm64;   ASSET="ppdm-aarch64" ;;
  *) echo "ppdm: 不支持的架构 $ARCH"; exit 1 ;;
esac

# ---- 下载 ----
URL="https://github.com/$REPO/releases/latest/download/$ASSET"
echo "ppdm: 下载 ppdm-$arch ($os) ..."
TMP="$(mktemp)"
if ! curl -fL --progress-bar --max-time 300 -o "$TMP" "$URL"; then
  rm -f "$TMP"
  echo "ppdm: 下载失败（$URL）"
  if [ "$ASSET" = "ppdm-aarch64" ]; then
    echo "ppdm: 若该资产暂未发布，可先构建（gcc src/ppdm.c ...）或稍后再试。"
  fi
  exit 1
fi
echo ""
chmod +x "$TMP"

# ---- 安装到 ~/.ppdm/bin/ppdm（与 ppdm update 一致）----
DEST="${HOME}/.ppdm/bin/ppdm"
mkdir -p "$(dirname "$DEST")"
cp "$TMP" "$DEST" && chmod +x "$DEST" && rm -f "$TMP"

# ---- 把 ~/.ppdm/bin prepend 进 ~/.bashrc ----
DEST_DIR="$(dirname "$DEST")"
RC="$HOME/.bashrc"
if [ -e "$RC" ] || [ -w "$HOME" ]; then
  if ! grep -qF "export PATH=\"$DEST_DIR:" "$RC" 2>/dev/null; then
    printf 'export PATH="%s:$PATH"\n' "$DEST_DIR" >> "$RC" 2>/dev/null || true
  fi
fi
hash -r 2>/dev/null || true

echo ""
echo "ppdm: 安装完成 -> $DEST"
"$DEST" -v 2>/dev/null || true

case ":$PATH:" in
  *":$DEST_DIR:"*) ;;
  *) echo "ppdm: 当前 shell 请执行: export PATH=\"$DEST_DIR:\$PATH\" && hash -r" ;;
esac
