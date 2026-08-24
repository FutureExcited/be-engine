#!/usr/bin/env bash
# Bootstrap macOS build dependencies for be-engine (Apple Silicon / Intel).
#
# be-engine renders through Vulkan; on macOS that runs via MoltenVK (Vulkan->Metal).
# Two dependency sets are needed:
#   1. Homebrew Vulkan loader + MoltenVK  (resolved at configure time from $HOMEBREW_PREFIX)
#   2. Shader Slang macOS binaries        (vendored under vendor/slang/macos-<arch>, gitignored)
#
# Re-run this any time CMake errors with "Missing Homebrew Vulkan loader" or
# "Missing Shader Slang macOS binaries".
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SLANG_VERSION="${SLANG_VERSION:-2026.3.1}"

arch="$(uname -m)"
case "$arch" in
  arm64|aarch64) SLANG_ARCH="aarch64"; SLANG_ASSET_ARCH="macos-aarch64" ;;
  x86_64)        SLANG_ARCH="x86_64";  SLANG_ASSET_ARCH="macos-x86_64"  ;;
  *) echo "Unsupported arch: $arch" >&2; exit 1 ;;
esac

echo "==> be-engine macOS deps (arch=$arch, slang=$SLANG_VERSION)"

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew not found. Install it from https://brew.sh and re-run." >&2
  exit 1
fi
echo "==> Installing Vulkan loader + MoltenVK via Homebrew (idempotent)"
brew install vulkan-loader molten-vk vulkan-headers vulkan-tools || true

BREW_PREFIX="${HOMEBREW_PREFIX:-$(brew --prefix)}"
for f in libvulkan.1.dylib libMoltenVK.dylib; do
  if [ ! -f "$BREW_PREFIX/lib/$f" ]; then
    echo "ERROR: $BREW_PREFIX/lib/$f missing after brew install." >&2
    exit 1
  fi
done
echo "    Vulkan loader + MoltenVK present under $BREW_PREFIX/lib"

SLANG_DIR="$REPO_ROOT/vendor/slang/$SLANG_ASSET_ARCH"
if [ -f "$SLANG_DIR/lib/libslang-compiler.dylib" ] && [ -f "$SLANG_DIR/include/slang.h" ]; then
  echo "==> Slang macOS binaries already present at $SLANG_DIR"
else
  echo "==> Downloading Slang $SLANG_VERSION ($SLANG_ASSET_ARCH)"
  url="https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-macos-${SLANG_ARCH}.zip"
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' EXIT
  curl -fL "$url" -o "$tmp/slang.zip"
  mkdir -p "$SLANG_DIR"
  ( cd "$tmp" && unzip -q slang.zip )
  cp -R "$tmp"/include "$SLANG_DIR/" 2>/dev/null || true
  cp -R "$tmp"/lib     "$SLANG_DIR/" 2>/dev/null || true
  cp -R "$tmp"/bin     "$SLANG_DIR/" 2>/dev/null || true
  if [ ! -f "$SLANG_DIR/lib/libslang-compiler.dylib" ]; then
    echo "ERROR: slang download did not yield lib/libslang-compiler.dylib." >&2
    echo "       Check the release asset name at the URL above." >&2
    exit 1
  fi
  echo "    Slang installed to $SLANG_DIR"
fi

echo "==> Done. Configure with: cmake --preset macos-debug"
