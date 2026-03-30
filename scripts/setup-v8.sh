#!/usr/bin/env bash
set -euo pipefail

VERSION="12.8.374"
DEST="v8"

if [ -f "$DEST/lib/libv8_monolith.a" ] && [ -f "$DEST/include/v8.h" ]; then
    echo "V8 already present at $DEST (version $VERSION)"
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
    arm64|aarch64) ARCH="arm64" ;;
    x86_64|amd64)  ARCH="x64" ;;
    *) echo "Unsupported architecture: $(uname -m)"; exit 1 ;;
esac

ASSET="v8-${OS}-${ARCH}-release.tar.gz"
URL="https://github.com/aspect-build/aspect-v8/releases/download/v${VERSION}/${ASSET}"

echo "Downloading V8 ${VERSION} (${OS}-${ARCH}) ..."
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

curl -fSL --retry 3 -o "${TMPDIR}/${ASSET}" "$URL"

echo "Extracting to ${DEST}/ ..."
rm -rf "$DEST"
mkdir -p "$DEST"
tar xzf "${TMPDIR}/${ASSET}" -C "$DEST"

# Validate expected structure
if [ ! -f "$DEST/lib/libv8_monolith.a" ]; then
    echo "ERROR: libv8_monolith.a not found after extraction"
    echo "  Expected: $DEST/lib/libv8_monolith.a"
    ls -la "$DEST/lib/" 2>/dev/null || echo "  $DEST/lib/ does not exist"
    exit 1
fi

echo "Done: V8 ${VERSION} (${OS}-${ARCH}) at ${DEST}/"
echo "  Headers:   ${DEST}/include/"
echo "  Libraries: ${DEST}/lib/libv8_monolith.a"
