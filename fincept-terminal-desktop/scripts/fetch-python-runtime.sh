#!/usr/bin/env bash
set -euo pipefail

# Download and install a standalone Python runtime for macOS or Linux into
# src-tauri/resources/python-{macos,linux}/python/bin/python3.
# Defaults are overridable via:
#   PYTHON_VERSION (e.g., 3.12.8)
#   PYTHON_STANDALONE_TAG (e.g., 20250115)
#   PYTHON_STANDALONE_BASE_URL
#   TARGET_ARCH (aarch64 | x86_64 | arm64)
#   PYTHON_ARCHIVE (full archive filename overrides autodetection)
#   FORCE_FETCH=1 (to overwrite an existing runtime)

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RESOURCES_DIR="$ROOT_DIR/src-tauri/resources"

PYTHON_VERSION="${PYTHON_VERSION:-3.12.8}" # Keep in sync with bundled Windows runtime (python312.dll reports 3.12.8)
PYTHON_STANDALONE_TAG="${PYTHON_STANDALONE_TAG:-20250115}"
PYTHON_STANDALONE_BASE_URL="${PYTHON_STANDALONE_BASE_URL:-https://github.com/astral-sh/python-build-standalone/releases/download}"

OS="$(uname -s)"
ARCH_RAW="${TARGET_ARCH:-$(uname -m)}"

case "$ARCH_RAW" in
  arm64) ARCH="aarch64" ;;
  aarch64) ARCH="aarch64" ;;
  x86_64) ARCH="x86_64" ;;
  *) ARCH="$ARCH_RAW" ;;
esac

case "$OS" in
  Darwin)
    TARGET_DIR="$RESOURCES_DIR/python-macos"
    if [[ "${ARCH}" == "aarch64" ]]; then
      ARCHIVES=(
        "cpython-${PYTHON_VERSION}+${PYTHON_STANDALONE_TAG}-aarch64-apple-darwin-install_only.tar.gz"
      )
    else
      ARCHIVES=(
        "cpython-${PYTHON_VERSION}+${PYTHON_STANDALONE_TAG}-x86_64-apple-darwin-install_only.tar.gz"
      )
    fi
    ;;
  Linux)
    TARGET_DIR="$RESOURCES_DIR/python-linux"
    if [[ "${ARCH}" == "aarch64" ]]; then
      ARCHIVES=(
        "cpython-${PYTHON_VERSION}+${PYTHON_STANDALONE_TAG}-aarch64-unknown-linux-gnu-install_only.tar.gz"
      )
    else
      ARCHIVES=(
        "cpython-${PYTHON_VERSION}+${PYTHON_STANDALONE_TAG}-x86_64-unknown-linux-gnu-install_only.tar.gz"
      )
    fi
    ;;
  *)
    echo "Unsupported OS for this fetch script: $OS (skipping)."
    exit 0
    ;;
esac

DEST_PYTHON_DIR="$TARGET_DIR/python"
TARGET_PYTHON="$DEST_PYTHON_DIR/bin/python3"

if [[ -x "$TARGET_PYTHON" && "${FORCE_FETCH:-0}" != "1" ]]; then
  echo "Bundled Python already present at $TARGET_PYTHON (set FORCE_FETCH=1 to refresh)."
  exit 0
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
ARCHIVE_CHOSEN=""

if [[ -n "${PYTHON_ARCHIVE:-}" ]]; then
  ARCHIVES=("$PYTHON_ARCHIVE")
fi

for ARCHIVE in "${ARCHIVES[@]}"; do
  URL="${PYTHON_STANDALONE_BASE_URL}/${PYTHON_STANDALONE_TAG}/${ARCHIVE}"
  echo "Attempting to fetch bundled Python ${PYTHON_VERSION} (${ARCH}) from:"
  echo "  $URL"
  if curl -fLsS "$URL" -o "$tmp_dir/python.tar.gz"; then
    ARCHIVE_CHOSEN="$ARCHIVE"
    break
  else
    echo "  -> not found (continuing to next archive name, if any)"
  fi
done

if [[ -z "$ARCHIVE_CHOSEN" ]]; then
  echo "Failed to download a bundled Python archive."
  echo "Tried:"
  for ARCHIVE in "${ARCHIVES[@]}"; do
    echo "  - ${PYTHON_STANDALONE_BASE_URL}/${PYTHON_STANDALONE_TAG}/${ARCHIVE}"
  done
  echo "Consider overriding PYTHON_ARCHIVE, TARGET_ARCH, PYTHON_STANDALONE_TAG, or PYTHON_VERSION."
  exit 1
fi

rm -rf "$DEST_PYTHON_DIR"
mkdir -p "$TARGET_DIR"

tar -xzf "$tmp_dir/python.tar.gz" -C "$tmp_dir"

if [[ -d "$tmp_dir/python/install" ]]; then
  mv "$tmp_dir/python/install" "$DEST_PYTHON_DIR"
elif [[ -d "$tmp_dir/python" ]]; then
  mv "$tmp_dir/python" "$DEST_PYTHON_DIR"
else
  echo "Unable to locate extracted Python payload in $tmp_dir"
  exit 1
fi

if [[ -x "$DEST_PYTHON_DIR/bin/python3.12" && ! -e "$DEST_PYTHON_DIR/bin/python3" ]]; then
  ln -s python3.12 "$DEST_PYTHON_DIR/bin/python3"
fi

echo "Bundled Python installed to: $TARGET_PYTHON"
echo "You can now run:"
echo "  $TARGET_PYTHON -m pip install -r $RESOURCES_DIR/requirements.txt"
