#!/bin/bash
# Install Deno by downloading the binary zip directly from GitHub releases
# and verifying its SHA256 checksum before extracting — no remote code execution.

set -euo pipefail

DENO_VERSION="${DENO_VERSION:-v2.3.3}"
DENO_INSTALL="${DENO_INSTALL:-$HOME/.deno}"

# Ensure unzip is available
if ! command -v unzip &>/dev/null; then
  sudo apt-get install -y unzip
fi

# Detect OS and architecture
OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
  Linux)
    case "$ARCH" in
      x86_64)  TARGET="x86_64-unknown-linux-gnu" ;;
      aarch64) TARGET="aarch64-unknown-linux-gnu" ;;
      *) echo "Unsupported architecture: $ARCH" >&2; exit 1 ;;
    esac
    ;;
  Darwin)
    case "$ARCH" in
      x86_64)        TARGET="x86_64-apple-darwin" ;;
      arm64|aarch64) TARGET="aarch64-apple-darwin" ;;
      *) echo "Unsupported architecture: $ARCH" >&2; exit 1 ;;
    esac
    ;;
  *) echo "Unsupported OS: $OS" >&2; exit 1 ;;
esac

ZIP_FILE="deno-${TARGET}.zip"
BASE_URL="https://github.com/denoland/deno/releases/download/${DENO_VERSION}"
ZIP_URL="${BASE_URL}/${ZIP_FILE}"
CHECKSUM_URL="${BASE_URL}/${ZIP_FILE}.sha256sum"

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

echo "Downloading Deno ${DENO_VERSION} for ${TARGET}..."
curl -fsSL "$ZIP_URL" -o "$TMPDIR/$ZIP_FILE"
curl -fsSL "$CHECKSUM_URL" -o "$TMPDIR/${ZIP_FILE}.sha256sum"

echo "Verifying SHA256 checksum..."
EXPECTED_HASH="$(awk '{print $1}' "$TMPDIR/${ZIP_FILE}.sha256sum")"
ACTUAL_HASH="$(sha256sum "$TMPDIR/$ZIP_FILE" | awk '{print $1}')"

if [ "$EXPECTED_HASH" != "$ACTUAL_HASH" ]; then
  echo "ERROR: SHA256 mismatch! The binary may have been tampered with." >&2
  echo "  expected: $EXPECTED_HASH" >&2
  echo "  actual:   $ACTUAL_HASH" >&2
  exit 1
fi

echo "Checksum verified. Installing..."
mkdir -p "$DENO_INSTALL/bin"
unzip -o "$TMPDIR/$ZIP_FILE" deno -d "$DENO_INSTALL/bin"
chmod +x "$DENO_INSTALL/bin/deno"

export DENO_INSTALL="$DENO_INSTALL"
export PATH="$PATH:$DENO_INSTALL/bin"

echo "Deno installed successfully. Run 'deno --version' to confirm."
