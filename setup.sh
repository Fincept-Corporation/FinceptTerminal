#!/usr/bin/env bash
set -euo pipefail

# ── Parse args ──────────────────────────────────────────────
CI_MODE=false
for arg in "$@"; do
    case "$arg" in
        --ci) CI_MODE=true ;;
    esac
done

# ── Pinned versions (must match CMakeLists.txt) ─────────────
QT_VERSION="6.8.3"
PYTHON_MIN="3.11"
CMAKE_MIN="3.27"
GCC_MIN="12.3"
CLANG_MIN="15.0"

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
echo "  Fincept Terminal v4.0.1 — Setup"
echo "  Pinned: Qt ${QT_VERSION} | CMake ${CMAKE_MIN}+ | Python ${PYTHON_MIN}+"
[ "$CI_MODE" = true ] && echo "  (CI mode — skipping interactive steps)"
echo "================================================"
echo ""

# ── Detect OS ───────────────────────────────────────────────
OS="$(uname -s)"
case "$OS" in
    Linux*)  PLATFORM="linux" ; QT_KIT="gcc_64"     ; PRESET="linux-release" ;;
    Darwin*) PLATFORM="macos" ; QT_KIT="clang_64"   ; PRESET="macos-release" ;;
    *)       fail "Unsupported OS: $OS" ;;
esac
echo "Platform: $OS"
echo ""

# ── Helper: version >= min ──────────────────────────────────
version_ge() {
    # $1=actual  $2=min    returns 0 (true) if actual >= min
    [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -1)" = "$2" ]
}

# ── Step 1: System dependencies (build tools only) ──────────
echo "[1/7] Installing system build tools..."
if [ "$PLATFORM" = "linux" ]; then
    command -v apt-get &>/dev/null || fail "apt-get not found. Install cmake / ninja-build / g++ / python3.11 / python3-pip manually."
    sudo apt-get update -qq
    sudo apt-get install -y --no-install-recommends \
        git cmake ninja-build g++ \
        python3 python3-pip python3-venv \
        libgl1-mesa-dev libglu1-mesa-dev \
        libxkbcommon-dev libxkbcommon-x11-dev \
        libfontconfig1 libdbus-1-3 \
        pkg-config curl
elif [ "$PLATFORM" = "macos" ]; then
    if ! command -v brew &>/dev/null; then
        [ "$CI_MODE" = true ] && fail "Homebrew not found in CI environment."
        info "Homebrew not found. Installing..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    brew install cmake ninja python@3.11 openssl@3 yt-dlp
fi
ok

# ── Step 2: Verify compiler version ─────────────────────────
echo "[2/7] Checking C++ compiler..."
if [ "$PLATFORM" = "linux" ]; then
    command -v g++ &>/dev/null || fail "g++ not found."
    GCC_VER="$(g++ -dumpfullversion -dumpversion 2>/dev/null || g++ --version | head -1 | awk '{print $NF}')"
    echo "  g++ ${GCC_VER}"
    version_ge "$GCC_VER" "$GCC_MIN" || fail "GCC ${GCC_MIN}+ required. Found ${GCC_VER}. Install g++-12 or newer."
elif [ "$PLATFORM" = "macos" ]; then
    command -v clang++ &>/dev/null || fail "clang++ not found. Run: xcode-select --install"
    CLANG_VER="$(clang++ --version | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)"
    echo "  Apple Clang ${CLANG_VER}"
    version_ge "$CLANG_VER" "$CLANG_MIN" || fail "Apple Clang ${CLANG_MIN}+ required (Xcode 15.2+). Found ${CLANG_VER}."
fi
ok

# ── Step 3: Verify CMake version ────────────────────────────
echo "[3/7] Checking CMake..."
command -v cmake &>/dev/null || fail "cmake not found."
CMAKE_VER="$(cmake --version | head -1 | awk '{print $3}')"
echo "  cmake ${CMAKE_VER}"
version_ge "$CMAKE_VER" "$CMAKE_MIN" || fail "CMake ${CMAKE_MIN}+ required. Found ${CMAKE_VER}. Download from https://cmake.org/download/"
ok

# ── Step 4: Verify Python version ───────────────────────────
echo "[4/7] Checking Python..."
PYTHON="$(command -v python3.11 || command -v python3 || true)"
[ -n "$PYTHON" ] || fail "python3 not found."
PY_VER="$($PYTHON -c 'import sys; print("%d.%d.%d" % sys.version_info[:3])')"
echo "  python ${PY_VER}"
version_ge "$PY_VER" "$PYTHON_MIN" || fail "Python ${PYTHON_MIN}+ required. Found ${PY_VER}."
ok

# ── Step 5: Install pinned Qt ${QT_VERSION} via aqtinstall ──
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_INSTALL_ROOT="${FINCEPT_QT_ROOT:-$SCRIPT_DIR/.qt}"
if [ "$PLATFORM" = "macos" ]; then
    QT_KIT_DIR="macos"
else
    QT_KIT_DIR="$QT_KIT"
fi

QT_PREFIX="$QT_INSTALL_ROOT/$QT_VERSION/$QT_KIT_DIR"

echo "[5/7] Locating Qt ${QT_VERSION}..."
if [ -n "${Qt6_DIR:-}" ] && [ -f "$Qt6_DIR/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    QT_PREFIX="$Qt6_DIR"
    info "Using Qt from Qt6_DIR env: $QT_PREFIX"
elif [ -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    info "Qt ${QT_VERSION} already installed at $QT_PREFIX"
else
    info "Installing Qt ${QT_VERSION} via aqtinstall to $QT_INSTALL_ROOT ..."
    # aqtinstall is a stable community tool that downloads exact Qt versions
    # from the official Qt mirror. Much smaller than Qt Online Installer and scriptable.
    "$PYTHON" -m pip install --user --quiet --upgrade aqtinstall
    AQT="$("$PYTHON" -m pip show aqtinstall >/dev/null 2>&1 && "$PYTHON" -m aqt help >/dev/null 2>&1 && echo "$PYTHON -m aqt" || echo "")"
    [ -n "$AQT" ] || fail "aqtinstall did not install correctly."
    # Qt host/target/arch
    if [ "$PLATFORM" = "linux" ]; then
        AQT_HOST="linux"   ; AQT_TARGET="desktop" ; AQT_ARCH="gcc_64"
    else
        AQT_HOST="mac"     ; AQT_TARGET="desktop" ; AQT_ARCH="macos"
    fi
    # Modules required to compile Fincept (match find_package COMPONENTS)
    AQT_MODULES="qtcharts qtwebsockets qtmultimedia qtspeech"
    $AQT install-qt "$AQT_HOST" "$AQT_TARGET" "$QT_VERSION" "$AQT_ARCH" \
        --outputdir "$QT_INSTALL_ROOT" \
        --modules $AQT_MODULES \
        || fail "aqtinstall failed. Check internet connection or install Qt ${QT_VERSION} manually from https://www.qt.io/download-qt-installer"
    [ -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ] || fail "Qt install completed but Qt6Config.cmake not found at $QT_PREFIX"
fi
export CMAKE_PREFIX_PATH="$QT_PREFIX${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
echo "  CMAKE_PREFIX_PATH=$QT_PREFIX"
ok

# ── Step 6: Configure (using CMake preset) ──────────────────
APP_DIR="$SCRIPT_DIR/fincept-qt"
[ -d "$APP_DIR" ] || fail "fincept-qt directory not found. Ensure you cloned the full repository."
cd "$APP_DIR"

echo "[6/7] Configuring (preset: $PRESET)..."
# Override the preset's default CMAKE_PREFIX_PATH with the one we just set,
# so the build picks up the aqtinstall location rather than ~/Qt/6.8.3/...
EXTRA_ARGS=""
if [ "$PLATFORM" = "macos" ] && [ -d "/opt/homebrew/opt/openssl@3" ]; then
    EXTRA_ARGS="-DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3"
fi

cmake --preset "$PRESET" -DCMAKE_PREFIX_PATH="$QT_PREFIX" $EXTRA_ARGS \
    || fail "CMake configure failed. See error above."
ok

# ── Step 7: Build ───────────────────────────────────────────
echo "[7/7] Compiling..."
cmake --build --preset "$PRESET" || fail "Build failed. See error above."
ok

# ── Done ────────────────────────────────────────────────────
BIN="$APP_DIR/build/$PRESET/FinceptTerminal"
[ "$PLATFORM" = "macos" ] && BIN="$APP_DIR/build/$PRESET/FinceptTerminal.app/Contents/MacOS/FinceptTerminal"

echo ""
echo "================================================"
echo "  Build complete!"
echo "  Run: $BIN"
echo "================================================"
echo ""

if [ "$CI_MODE" = true ]; then
    exit 0
fi

read -r -p "  Launch now? (y/n): " LAUNCH
if [[ "$LAUNCH" =~ ^[Yy]$ ]]; then
    "$BIN"
fi
