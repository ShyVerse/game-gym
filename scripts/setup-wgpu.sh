#!/usr/bin/env bash
set -euo pipefail

VERSION="v24.0.3.1"
DEST="wgpu-native"

if [ -d "$DEST/lib" ] && [ -d "$DEST/include" ]; then
    echo "wgpu-native already present at $DEST"
    exit 0
fi

# Detect OS
case "$(uname -s)" in
    Darwin)  OS="macos" ;;
    Linux)   OS="linux" ;;
    MINGW*|MSYS*|CYGWIN*) OS="windows" ;;
    *) echo "Unsupported OS: $(uname -s)"; exit 1 ;;
esac

# Detect architecture
case "$(uname -m)" in
    arm64|aarch64) ARCH="aarch64" ;;
    x86_64|amd64)  ARCH="x86_64" ;;
    *) echo "Unsupported architecture: $(uname -m)"; exit 1 ;;
esac

ASSET="wgpu-${OS}-${ARCH}-release.zip"
URL="https://github.com/gfx-rs/wgpu-native/releases/download/${VERSION}/${ASSET}"

echo "Downloading wgpu-native ${VERSION} (${OS}-${ARCH}) ..."
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

curl -fSL --retry 3 -o "${TMPDIR}/${ASSET}" "$URL"

echo "Extracting to ${DEST}/ ..."
rm -rf "$DEST"
mkdir -p "$DEST"
unzip -q "${TMPDIR}/${ASSET}" -d "$DEST"

echo "Done: wgpu-native ${VERSION} (${OS}-${ARCH}) at ${DEST}/"
echo "  Headers: ${DEST}/include/webgpu/"
echo "  Libraries: ${DEST}/lib/"
