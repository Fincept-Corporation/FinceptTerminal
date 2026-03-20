#!/usr/bin/env bash
set -euo pipefail

# ── Parse args ──────────────────────────────────────────────
CI_MODE=false
for arg in "$@"; do
    case "$arg" in
        --ci) CI_MODE=true ;;
    esac
done

# ── Colours ─────────────────────────────────────────────────
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

ok()   { echo -e "  ${GREEN}OK${NC}"; }
fail() { echo -e "  ${RED}ERROR: $1${NC}"; exit 1; }
info() { echo -e "  ${YELLOW}$1${NC}"; }

echo ""
echo "================================================"
echo "  Fincept Terminal — Setup"
[ "$CI_MODE" = true ] && echo "  (CI mode — skipping interactive steps)"
echo "================================================"
echo ""

# ── Detect OS ───────────────────────────────────────────────
OS="$(uname -s)"
case "$OS" in
    Linux*)  PLATFORM="linux" ;;
    Darwin*) PLATFORM="macos" ;;
    *)       fail "Unsupported OS: $OS" ;;
esac
echo "Platform: $OS"
echo ""

# ── Step 1: System dependencies ─────────────────────────────
echo "[1/5] Installing system dependencies..."
if [ "$PLATFORM" = "linux" ]; then
    if ! command -v apt-get &>/dev/null; then
        fail "apt-get not found. Please install dependencies manually (see docs/GETTING_STARTED.md)"
    fi
    sudo apt-get update -qq
    sudo apt-get install -y \
        git cmake ninja-build g++ python3 python3-pip \
        libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev \
        libxrandr-dev libxi-dev libxext-dev libxfixes-dev \
        libwayland-dev libxkbcommon-dev pkg-config \
        autoconf automake libtool
elif [ "$PLATFORM" = "macos" ]; then
    if [ "$CI_MODE" = false ] && ! command -v brew &>/dev/null; then
        info "Homebrew not found. Installing..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    # In CI, brew is pre-installed on macOS runners
    brew install cmake ninja pkg-config autoconf automake libtool python
fi
ok

# ── Step 2: Check C++ compiler ──────────────────────────────
echo "[2/5] Checking C++ compiler..."
if [ "$PLATFORM" = "linux" ]; then
    command -v g++ &>/dev/null || fail "g++ not found. Please check apt output above."
elif [ "$PLATFORM" = "macos" ]; then
    command -v clang++ &>/dev/null || fail "clang++ not found. Run: xcode-select --install"
fi
ok

# ── Step 3: vcpkg ───────────────────────────────────────────
echo "[3/5] Setting up vcpkg..."
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"

if [ ! -f "$VCPKG_ROOT/vcpkg" ]; then
    info "vcpkg not found. Cloning into $VCPKG_ROOT..."
    git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
    "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
    info "vcpkg installed at $VCPKG_ROOT"
else
    info "vcpkg already installed at $VCPKG_ROOT"
fi

export VCPKG_ROOT

# Persist VCPKG_ROOT in shell profile (skip in CI — env var is already set)
if [ "$CI_MODE" = false ]; then
    SHELL_PROFILE=""
    if [ -f "$HOME/.zshrc" ]; then
        SHELL_PROFILE="$HOME/.zshrc"
    elif [ -f "$HOME/.bashrc" ]; then
        SHELL_PROFILE="$HOME/.bashrc"
    elif [ -f "$HOME/.bash_profile" ]; then
        SHELL_PROFILE="$HOME/.bash_profile"
    fi
    if [ -n "$SHELL_PROFILE" ]; then
        if ! grep -q "VCPKG_ROOT" "$SHELL_PROFILE"; then
            echo "" >> "$SHELL_PROFILE"
            echo "export VCPKG_ROOT=\"$VCPKG_ROOT\"" >> "$SHELL_PROFILE"
            info "Added VCPKG_ROOT to $SHELL_PROFILE"
        fi
    fi
fi
ok

# ── Step 4: Configure ────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$SCRIPT_DIR/fincept-cpp"
[ -d "$APP_DIR" ] || fail "fincept-cpp directory not found. Make sure you cloned the full repository."
cd "$APP_DIR"

echo "[4/5] Configuring (vcpkg downloads dependencies — may take 10-20 min on first run)..."
cmake --preset=default \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
ok

# ── Step 5: Build ────────────────────────────────────────────
echo "[5/5] Compiling..."
cmake --build build --config Release --parallel
ok

# ── Done ─────────────────────────────────────────────────────
echo ""
echo "================================================"
echo "  Build complete!"
echo "================================================"
echo ""
echo "  Run the terminal:"
echo "    ./build/FinceptTerminal"
echo ""

# Skip launch prompt in CI
if [ "$CI_MODE" = false ]; then
    read -r -p "  Launch now? (y/n): " LAUNCH
    if [[ "$LAUNCH" =~ ^[Yy]$ ]]; then
        if [ "$PLATFORM" = "macos" ]; then
            open ./build/FinceptTerminal 2>/dev/null || ./build/FinceptTerminal
        else
            ./build/FinceptTerminal
        fi
    fi
fi
