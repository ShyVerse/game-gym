#!/usr/bin/env bash
set -euo pipefail
VERSION="v1.92.6-docking"
DEST="third_party/imgui"
if [ -d "$DEST" ] && [ -f "$DEST/imgui.h" ]; then
    echo "ImGui already present at $DEST"; exit 0
fi
echo "Downloading Dear ImGui $VERSION ..."
rm -rf "$DEST"
git clone --depth 1 --branch "$VERSION" https://github.com/ocornut/imgui.git "$DEST"
rm -rf "$DEST/.git" "$DEST/.github" "$DEST/examples" "$DEST/misc"
echo "Done: ImGui $VERSION at $DEST"
