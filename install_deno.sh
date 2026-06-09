#!/bin/bash
# Install Deno using a two-step download + SHA256 integrity check
# instead of piping directly from curl to sh.

set -euo pipefail

DENO_VERSION="${DENO_VERSION:-v2.3.3}"
DENO_INSTALL="${DENO_INSTALL:-$HOME/.deno}"
INSTALLER_URL="https://deno.land/install.sh"
CHECKSUM_URL="https://deno.land/install.sh.sha256"

# Ensure unzip is available (required by the Deno installer)
sudo apt-get install -y unzip

# Download the installer script and its published SHA256 checksum
TMPDIR="$(mktemp -d)"
INSTALLER_FILE="$TMPDIR/install.sh"
CHECKSUM_FILE="$TMPDIR/install.sh.sha256"

echo "Downloading Deno installer..."
curl -fsSL "$INSTALLER_URL" -o "$INSTALLER_FILE"
curl -fsSL "$CHECKSUM_URL" -o "$CHECKSUM_FILE"

# Verify integrity before executing anything
echo "Verifying SHA256 checksum..."
# The published checksum file contains "HASH  filename"; normalise to check against our local file
EXPECTED_HASH="$(awk '{print $1}' "$CHECKSUM_FILE")"
ACTUAL_HASH="$(sha256sum "$INSTALLER_FILE" | awk '{print $1}')"

if [ "$EXPECTED_HASH" != "$ACTUAL_HASH" ]; then
  echo "ERROR: SHA256 mismatch! The installer may have been tampered with." >&2
  echo "  expected: $EXPECTED_HASH" >&2
  echo "  actual:   $ACTUAL_HASH" >&2
  rm -rf "$TMPDIR"
  exit 1
fi

echo "Checksum verified. Running installer..."
sh "$INSTALLER_FILE"

# Clean up
rm -rf "$TMPDIR"

export DENO_INSTALL="$DENO_INSTALL"
export PATH="$PATH:$DENO_INSTALL/bin"

echo "Deno installed successfully. Run 'deno --version' to confirm."
