#!/bin/bash
# Download every PMM release asset (except the huge mirror.zip snapshots) into
# web/releases/<tag>/ so the git.php Releases page links to this server.
set -u
REPO=JGZYES/ParlzPackageManger
DEST="web/releases"
PATTERNS=(-p 'pmm' -p 'pmm.exe' -p 'pmm-*.zip' -p 'install.sh' -p '*_amd64.deb' -p '*_amd64.rpm' -p '*.pdm' -p 'pdm' -p 'pdm.exe')
mkdir -p "$DEST"
for tag in $(gh api "repos/$REPO/releases" --paginate -q '.[].tag_name'); do
  echo "== $tag =="
  mkdir -p "$DEST/$tag"
  gh release download "$tag" -R "$REPO" -D "$DEST/$tag" "${PATTERNS[@]}" 2>&1 | tail -1 || echo "  (skip/err $tag)"
done
echo "done. Total: $(find "$DEST" -type f | wc -l) files, $(du -sh "$DEST" | cut -f1)"
