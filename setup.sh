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
echo "  Fincept Terminal v4.0.0 — Setup"
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
    command -v apt-get &>/dev/null || fail "apt-get not found. Install dependencies manually (see docs/GETTING_STARTED.md)"
    sudo apt-get update -qq
    sudo apt-get install -y \
        git cmake g++ python3 python3-pip \
        qt6-base-dev qt6-charts-dev qt6-tools-dev \
        libqt6sql6-sqlite libqt6websockets6-dev \
        libgl1-mesa-dev libglu1-mesa-dev \
        pkg-config
elif [ "$PLATFORM" = "macos" ]; then
    if ! command -v brew &>/dev/null; then
        [ "$CI_MODE" = true ] && fail "Homebrew not found in CI environment."
        info "Homebrew not found. Installing..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    brew install cmake qt python3
fi
ok

# ── Step 2: Check C++ compiler ──────────────────────────────
echo "[2/5] Checking C++ compiler..."
if [ "$PLATFORM" = "linux" ]; then
    command -v g++ &>/dev/null || fail "g++ not found. Check apt output above."
    g++ --version | head -1
elif [ "$PLATFORM" = "macos" ]; then
    command -v clang++ &>/dev/null || fail "clang++ not found. Run: xcode-select --install"
    clang++ --version | head -1
fi
ok

# ── Step 3: Locate Qt6 ──────────────────────────────────────
echo "[3/5] Locating Qt6..."
QT_PREFIX=""
if [ -n "${Qt6_DIR:-}" ]; then
    QT_PREFIX="$Qt6_DIR"
elif [ "$PLATFORM" = "linux" ]; then
    QT_PREFIX="/usr"
    command -v qmake6 &>/dev/null || command -v qmake &>/dev/null || \
        fail "Qt6 not found. Run: sudo apt install qt6-base-dev"
elif [ "$PLATFORM" = "macos" ]; then
    QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
    [ -z "$QT_PREFIX" ] && fail "Qt6 not found. Run: brew install qt"
    info "Qt6 found at $QT_PREFIX"
fi
ok

# ── Step 4: Configure ────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$SCRIPT_DIR/fincept-qt"
[ -d "$APP_DIR" ] || fail "fincept-qt directory not found. Ensure you cloned the full repository."
cd "$APP_DIR"

echo "[4/5] Configuring..."
if [ "$PLATFORM" = "macos" ]; then
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_PREFIX"
else
    cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
ok

# ── Step 5: Build ────────────────────────────────────────────
echo "[5/5] Compiling..."
cmake --build build --config Release --parallel
ok

# ── Done ─────────────────────────────────────────────────────
echo ""
echo "================================================"
echo "  Build complete!"
echo "  Run: ./build/FinceptTerminal"
echo "================================================"
echo ""

if [ "$CI_MODE" = true ]; then
    exit 0
fi

read -r -p "  Launch now? (y/n): " LAUNCH
if [[ "$LAUNCH" =~ ^[Yy]$ ]]; then
    if [ "$PLATFORM" = "macos" ]; then
        open ./build/FinceptTerminal 2>/dev/null || ./build/FinceptTerminal
    else
        ./build/FinceptTerminal
    fi
fi
