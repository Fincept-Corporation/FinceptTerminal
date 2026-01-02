#!/bin/bash
# Fincept Terminal - Development Environment Setup Script
# Supports: Linux (Ubuntu/Debian/Fedora/Arch), macOS

set -e

echo ""
echo "========================================"
echo "  Fincept Terminal Setup Script"
echo "  Linux / macOS Development Environment"
echo "========================================"
echo ""

# Detect OS
case "$(uname -s)" in
    Linux*)  OS="linux";;
    Darwin*) OS="mac";;
    *)       echo "Error: Use setup.bat for Windows"; exit 1;;
esac
echo "Detected OS: $OS"

# Detect Linux distro
detect_distro() {
    [ -f /etc/os-release ] && . /etc/os-release && echo $ID || echo "unknown"
}

# Install Linux dependencies
install_linux() {
    DISTRO=$(detect_distro)
    echo "Installing dependencies for $DISTRO..."
    case $DISTRO in
        ubuntu|debian|pop)
            sudo apt update
            # Check Ubuntu version - Tauri 2.x requires Ubuntu 22.04+
            VERSION=$(lsb_release -rs 2>/dev/null | cut -d. -f1)
            if [ "$VERSION" -lt 22 ] 2>/dev/null; then
                echo ""
                echo "ERROR: Ubuntu $VERSION is not supported!"
                echo "Tauri 2.x requires Ubuntu 22.04+ (glib 2.70+)"
                echo "Please upgrade to Ubuntu 22.04 or newer."
                echo ""
                exit 1
            fi
            # Ubuntu/Debian 22.04+ dependencies
            sudo apt install -y libwebkit2gtk-4.1-dev build-essential curl wget \
                file libxdo-dev libssl-dev libayatana-appindicator3-dev librsvg2-dev \
                pkg-config libgtk-3-dev libglib2.0-dev libsoup-3.0-dev \
                libjavascriptcoregtk-4.1-dev python3 python3-dev python3-venv
            # Set PYO3_PYTHON
            export PYO3_PYTHON=$(which python3)
            echo "export PYO3_PYTHON=$(which python3)" >> ~/.bashrc ;;
        fedora)
            sudo dnf check-update || true
            sudo dnf install -y webkit2gtk4.1-devel openssl-devel curl wget file \
                libappindicator-gtk3-devel librsvg2-devel libxdo-devel \
                python3 python3-devel
            sudo dnf group install -y "C Development Tools and Libraries"
            export PYO3_PYTHON=$(which python3)
            echo "export PYO3_PYTHON=$(which python3)" >> ~/.bashrc ;;
        arch|manjaro)
            sudo pacman -Syu --noconfirm
            sudo pacman -S --needed --noconfirm webkit2gtk-4.1 base-devel curl wget \
                file openssl appmenu-gtk-module libappindicator-gtk3 librsvg xdotool python
            export PYO3_PYTHON=$(which python3)
            echo "export PYO3_PYTHON=$(which python3)" >> ~/.bashrc ;;
        *)  echo "Unknown distro. Install manually: build-essential gtk3 webkit2gtk libssl" ;;
    esac
}

# Install macOS dependencies
install_mac() {
    echo "Installing macOS dependencies..."
    command -v brew &>/dev/null || /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    xcode-select --install 2>/dev/null || true

    # Install Python 3.12 for PyO3
    brew install python@3.12

    # Set Python environment for PyO3
    PYTHON_PATH=$(brew --prefix python@3.12)
    export PYO3_PYTHON="$PYTHON_PATH/bin/python3.12"
    echo "export PYO3_PYTHON=\"$PYTHON_PATH/bin/python3.12\"" >> ~/.zshrc
    echo "export PYO3_PYTHON=\"$PYTHON_PATH/bin/python3.12\"" >> ~/.bashrc
    echo "[OK] Python configured for PyO3: $PYO3_PYTHON"
}

# Install Rust
install_rust() {
    if command -v rustc &>/dev/null; then
        echo "[OK] Rust: $(rustc --version)"
    else
        echo "[..] Installing Rust..."
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
        source "$HOME/.cargo/env"
    fi
}

# Install Bun
install_bun() {
    if command -v bun &>/dev/null; then
        echo "[OK] Bun: $(bun --version)"
    else
        echo "[..] Installing Bun..."
        curl -fsSL https://bun.sh/install | bash
        export BUN_INSTALL="$HOME/.bun" && export PATH="$BUN_INSTALL/bin:$PATH"
    fi
}

# Ensure curl is installed
ensure_curl() {
    if ! command -v curl &>/dev/null; then
        echo "[..] Installing curl..."
        case "$OS" in
            linux)
                sudo apt install -y curl 2>/dev/null || sudo dnf install -y curl 2>/dev/null || sudo pacman -S --noconfirm curl
                ;;
            mac)
                echo "curl should be pre-installed on macOS"
                ;;
        esac
    fi
    echo "[OK] curl available"
}

# Main
ensure_curl
[ "$OS" = "linux" ] && install_linux || install_mac
install_rust
install_bun

# Ensure bun and rust are in PATH
export BUN_INSTALL="$HOME/.bun"
export PATH="$BUN_INSTALL/bin:$HOME/.cargo/bin:$PATH"

echo "[..] Installing project dependencies..."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../fincept-terminal-desktop" 2>/dev/null || cd "$SCRIPT_DIR/.."
bun install
bun add -d @tauri-apps/cli

echo ""
echo "========================================"
echo "  Setup Complete!"
echo "  Run: cd fincept-terminal-desktop"
echo "       bun run tauri:dev"
echo "========================================"
