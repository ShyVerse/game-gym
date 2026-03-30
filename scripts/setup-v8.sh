#!/usr/bin/env bash
set -euo pipefail

# V8 monolith build from just-js/v8 — prebuilt static archive with headers.
VERSION="14.3"
DEST="v8"

if [ -f "$DEST/lib/libv8_monolith.a" ] && [ -f "$DEST/include/v8.h" ]; then
    echo "V8 already present at $DEST (version $VERSION)"
    exit 0
fi

# Detect OS
case "$(uname -s)" in
    Darwin)  OS="mac" ;;
    Linux)   OS="linux" ;;
    *) echo "Unsupported OS: $(uname -s)"; exit 1 ;;
esac

# Detect architecture
case "$(uname -m)" in
    arm64|aarch64) ARCH="arm64" ;;
    x86_64|amd64)  ARCH="x64" ;;
    *) echo "Unsupported architecture: $(uname -m)"; exit 1 ;;
esac

BASE_URL="https://github.com/just-js/v8/releases/download/${VERSION}"
LIB_ASSET="libv8_monolith-${OS}-${ARCH}.a.gz"
HEADERS_ASSET="include.tar.gz"

echo "Downloading V8 ${VERSION} (${OS}-${ARCH}) ..."
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

curl -fSL --retry 3 -o "${WORK_DIR}/${LIB_ASSET}" "${BASE_URL}/${LIB_ASSET}"
curl -fSL --retry 3 -o "${WORK_DIR}/${HEADERS_ASSET}" "${BASE_URL}/${HEADERS_ASSET}"

echo "Extracting to ${DEST}/ ..."
rm -rf "$DEST"
mkdir -p "$DEST/lib"

# Library: decompress gzipped static archive
gunzip -c "${WORK_DIR}/${LIB_ASSET}" > "$DEST/lib/libv8_monolith.a"

# Headers: extract include/ directory
tar xzf "${WORK_DIR}/${HEADERS_ASSET}" -C "$DEST"

# Validate expected structure
if [ ! -f "$DEST/lib/libv8_monolith.a" ]; then
    echo "ERROR: libv8_monolith.a not found after extraction"
    exit 1
fi
if [ ! -f "$DEST/include/v8.h" ]; then
    echo "ERROR: v8.h not found after extraction"
    exit 1
fi

echo "Done: V8 ${VERSION} (${OS}-${ARCH}) at ${DEST}/"
echo "  Headers:   ${DEST}/include/"
echo "  Libraries: ${DEST}/lib/libv8_monolith.a"
