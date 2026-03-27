#!/usr/bin/env bash
set -euo pipefail

VERSION="v24.0.3.1"
BASE_URL="https://github.com/gfx-rs/wgpu-native/releases/download/${VERSION}"
TARGET_DIR="$(cd "$(dirname "$0")/.." && pwd)/wgpu-native"

detect_platform() {
  local os arch
  os="$(uname -s)"
  arch="$(uname -m)"

  case "$os" in
    Darwin)
      case "$arch" in
        arm64) echo "macos-aarch64" ;;
        x86_64) echo "macos-x86_64" ;;
      esac ;;
    Linux)
      case "$arch" in
        x86_64) echo "linux-x86_64" ;;
        aarch64) echo "linux-aarch64" ;;
      esac ;;
    MINGW*|MSYS*|CYGWIN*)
      echo "windows-x86_64" ;;
  esac
}

PLATFORM="$(detect_platform)"
if [ -z "$PLATFORM" ]; then
  echo "Error: unsupported platform $(uname -s)/$(uname -m)"
  exit 1
fi

FILENAME="wgpu-${PLATFORM}-release.zip"
URL="${BASE_URL}/${FILENAME}"

echo "Downloading wgpu-native ${VERSION} for ${PLATFORM}..."
mkdir -p "$TARGET_DIR"
curl -L "$URL" -o "/tmp/${FILENAME}"
unzip -o "/tmp/${FILENAME}" -d "$TARGET_DIR"
rm "/tmp/${FILENAME}"

mkdir -p "$TARGET_DIR/include" "$TARGET_DIR/lib"
mv "$TARGET_DIR"/*.h "$TARGET_DIR/include/" 2>/dev/null || true
mv "$TARGET_DIR"/*.a "$TARGET_DIR/lib/" 2>/dev/null || true
mv "$TARGET_DIR"/*.dylib "$TARGET_DIR/lib/" 2>/dev/null || true
mv "$TARGET_DIR"/*.so "$TARGET_DIR/lib/" 2>/dev/null || true
mv "$TARGET_DIR"/*.dll "$TARGET_DIR/lib/" 2>/dev/null || true
mv "$TARGET_DIR"/*.lib "$TARGET_DIR/lib/" 2>/dev/null || true

echo "wgpu-native installed to $TARGET_DIR"
echo "  Headers: $TARGET_DIR/include/"
echo "  Libs:    $TARGET_DIR/lib/"
