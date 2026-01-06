#!/bin/bash
# Fincept Terminal - Development Environment Setup Script
# Supports: Linux (Ubuntu/Debian/Fedora/Arch), macOS
# NO SUDO REQUIRED - All user-level installations

set -e

echo ""
echo "========================================"
echo "  Fincept Terminal Setup Script"
echo "  Linux / macOS Development Environment"
echo "  (No Sudo/Admin Required)"
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

# Check Linux dependencies (no installation, just verification)
check_linux_deps() {
    DISTRO=$(detect_distro)
    echo "Checking dependencies for $DISTRO..."

    # Check Ubuntu version
    if [ "$DISTRO" = "ubuntu" ] || [ "$DISTRO" = "debian" ] || [ "$DISTRO" = "pop" ]; then
        VERSION=$(lsb_release -rs 2>/dev/null | cut -d. -f1)
        if [ "$VERSION" -lt 22 ] 2>/dev/null; then
            echo ""
            echo "ERROR: Ubuntu $VERSION is not supported!"
            echo "Tauri 2.x requires Ubuntu 22.04+ (glib 2.70+)"
            echo "Please upgrade to Ubuntu 22.04 or newer."
            echo ""
            exit 1
        fi
    fi

    # Check for essential tools
    MISSING_DEPS=""

    if ! command -v gcc &>/dev/null; then
        MISSING_DEPS="$MISSING_DEPS gcc"
    fi

    if ! command -v pkg-config &>/dev/null; then
        MISSING_DEPS="$MISSING_DEPS pkg-config"
    fi

    if ! command -v python3 &>/dev/null; then
        MISSING_DEPS="$MISSING_DEPS python3"
    fi

    if [ -n "$MISSING_DEPS" ]; then
        echo ""
        echo "[!!] Missing system dependencies:$MISSING_DEPS"
        echo ""
        echo "Please install them using your package manager:"
        case $DISTRO in
            ubuntu|debian|pop)
                echo "  sudo apt install -y build-essential pkg-config libssl-dev python3 python3-pip"
                echo "  sudo apt install -y libwebkit2gtk-4.1-dev libgtk-3-dev libayatana-appindicator3-dev"
                ;;
            fedora)
                echo "  sudo dnf install -y gcc pkg-config openssl-devel python3 python3-pip"
                echo "  sudo dnf install -y webkit2gtk4.1-devel gtk3-devel"
                ;;
            arch|manjaro)
                echo "  sudo pacman -S base-devel pkg-config openssl python python-pip"
                echo "  sudo pacman -S webkit2gtk-4.1 gtk3"
                ;;
        esac
        echo ""
        echo "After installing dependencies, re-run this script."
        echo ""
        exit 1
    fi

    echo "[OK] Essential build tools found"

    # Setup Python
    if command -v python3 &>/dev/null; then
        python3 -m pip install --upgrade pip setuptools wheel --user 2>/dev/null || echo "[!] Pip upgrade skipped"

        PYTHON_PATH=$(which python3)
        export PYO3_PYTHON="$PYTHON_PATH"
        if ! grep -q "export PYO3_PYTHON=" ~/.bashrc 2>/dev/null; then
            echo "export PYO3_PYTHON=\"$PYTHON_PATH\"" >> ~/.bashrc
        fi
        echo "[OK] PYO3_PYTHON set to $PYTHON_PATH"
    fi
}

# Setup macOS (no admin required, uses Homebrew)
setup_mac() {
    echo "Setting up macOS dependencies..."

    # Install Homebrew if not present (no sudo needed)
    if ! command -v brew &>/dev/null; then
        echo "[..] Installing Homebrew (no admin required)..."
        NONINTERACTIVE=1 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

        # Add Homebrew to PATH for current session
        if [ -f "/opt/homebrew/bin/brew" ]; then
            eval "$(/opt/homebrew/bin/brew shellenv)"
        elif [ -f "/usr/local/bin/brew" ]; then
            eval "$(/usr/local/bin/brew shellenv)"
        fi

        # Verify installation
        if ! command -v brew &>/dev/null; then
            echo "[!!] Homebrew installation failed"
            exit 1
        fi
    fi
    echo "[OK] Homebrew: $(brew --version | head -n1)"

    # Check for Xcode Command Line Tools
    if ! xcode-select -p &>/dev/null; then
        echo ""
        echo "[!!] Xcode Command Line Tools not found!"
        echo "     These are REQUIRED for building Rust/Tauri apps."
        echo ""
        echo "Install with: xcode-select --install"
        echo "(This may require admin password)"
        echo ""
        echo "After installing, re-run this script."
        exit 1
    fi
    echo "[OK] Xcode Command Line Tools installed"

    # Install Python 3.12 if not present
    if ! command -v python3 &>/dev/null || ! brew list python@3.12 &>/dev/null; then
        echo "[..] Installing Python 3.12 via Homebrew..."
        brew install python@3.12
    fi

    # Use Homebrew Python
    if brew --prefix python@3.12 &>/dev/null; then
        PYTHON_PATH="$(brew --prefix python@3.12)/bin/python3.12"
        echo "[OK] Python 3.12: $(brew list --versions python@3.12)"
    else
        PYTHON_PATH=$(which python3)
        echo "[OK] Python 3: $(python3 --version)"
    fi

    # Upgrade pip (user-level)
    $PYTHON_PATH -m pip install --upgrade pip setuptools wheel --user 2>/dev/null || echo "[!] Pip upgrade skipped"

    # Set Python environment for PyO3
    export PYO3_PYTHON="$PYTHON_PATH"

    # Add to shell configs
    for shellrc in ~/.zshrc ~/.bashrc; do
        touch "$shellrc"
        if ! grep -q "export PYO3_PYTHON=" "$shellrc"; then
            echo "export PYO3_PYTHON=\"$PYTHON_PATH\"" >> "$shellrc"
        fi
    done
    echo "[OK] PYO3_PYTHON set to $PYO3_PYTHON"
}

# Install Rust
install_rust() {
    if command -v rustc &>/dev/null; then
        echo "[OK] Rust: $(rustc --version)"
    else
        echo "[..] Installing Rust..."
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain stable

        # Source cargo environment for current session
        if [ -f "$HOME/.cargo/env" ]; then
            source "$HOME/.cargo/env"
        fi

        # Verify installation
        if command -v rustc &>/dev/null; then
            echo "[OK] Rust installed: $(rustc --version)"
        else
            echo "[!!] Rust installed but not in PATH. Restart terminal and re-run script."
            exit 1
        fi
    fi
}

# Install Bun
install_bun() {
    if command -v bun &>/dev/null; then
        echo "[OK] Bun: $(bun --version)"
    else
        echo "[..] Installing Bun..."
        curl -fsSL https://bun.sh/install | bash

        # Source bun environment for current session
        export BUN_INSTALL="$HOME/.bun"
        export PATH="$BUN_INSTALL/bin:$PATH"

        # Add to shell configs
        for shellrc in ~/.bashrc ~/.zshrc; do
            if [ -f "$shellrc" ]; then
                if ! grep -q "BUN_INSTALL=" "$shellrc"; then
                    echo 'export BUN_INSTALL="$HOME/.bun"' >> "$shellrc"
                    echo 'export PATH="$BUN_INSTALL/bin:$PATH"' >> "$shellrc"
                fi
            fi
        done

        # Verify installation
        if command -v bun &>/dev/null; then
            echo "[OK] Bun installed: $(bun --version)"
        else
            echo "[!!] Bun installed but not in PATH. Restart terminal and re-run script."
            exit 1
        fi
    fi
}

# Check for curl (should be pre-installed)
check_curl() {
    if ! command -v curl &>/dev/null; then
        echo ""
        echo "[!!] curl not found!"
        echo "     curl is required for downloading dependencies."
        echo ""
        if [ "$OS" = "linux" ]; then
            echo "Install with: sudo apt install curl   (or dnf/pacman)"
        else
            echo "curl should be pre-installed on macOS"
        fi
        echo ""
        exit 1
    fi
    echo "[OK] curl available"
}

# Install Python packages
install_python_packages() {
    echo ""
    echo "[..] Installing Python packages for analytics..."

    if [ "$OS" = "mac" ]; then
        PYTHON_CMD="$(brew --prefix python@3.12)/bin/python3.12"
    else
        PYTHON_CMD="python3"
    fi

    $PYTHON_CMD -m pip install --upgrade pip setuptools wheel --user
    $PYTHON_CMD -m pip install pandas numpy yfinance requests langchain-ollama ccxt sqlalchemy --user

    echo "[OK] Python packages installed"
}

# Main execution
check_curl

# Check/Setup platform-specific dependencies
if [ "$OS" = "linux" ]; then
    check_linux_deps
else
    setup_mac
fi

install_rust
install_bun

# Ensure bun and rust are in PATH for current session
export BUN_INSTALL="$HOME/.bun"
export PATH="$BUN_INSTALL/bin:$HOME/.cargo/bin:$PATH"

# Install Python packages
install_python_packages

# Navigate to project directory
echo ""
echo "[..] Navigating to project directory..."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -d "$SCRIPT_DIR/../fincept-terminal-desktop" ]; then
    cd "$SCRIPT_DIR/../fincept-terminal-desktop"
    echo "[OK] Found fincept-terminal-desktop directory"
elif [ -d "$SCRIPT_DIR/.." ]; then
    cd "$SCRIPT_DIR/.."
    if [ ! -f "package.json" ]; then
        echo "[!!] fincept-terminal-desktop directory not found!"
        echo "    Current directory: $(pwd)"
        exit 1
    fi
else
    echo "[!!] Could not find project directory!"
    exit 1
fi

# Install project dependencies
echo ""
echo "[..] Installing project dependencies..."
if ! bun install; then
    echo "[!!] Failed to install dependencies with bun"
    exit 1
fi
echo "[OK] Dependencies installed"

echo "[..] Adding Tauri CLI..."
if ! bun add -d @tauri-apps/cli; then
    echo "[!!] Failed to add Tauri CLI"
    exit 1
fi
echo "[OK] Tauri CLI added"

echo ""
echo "========================================"
echo "  Setup Complete!"
echo "========================================"
echo ""
echo "Next steps:"
echo "  1. RESTART your terminal or run:"
if [ "$OS" = "linux" ]; then
    echo "     source ~/.bashrc"
else
    echo "     source ~/.zshrc  # or source ~/.bashrc"
fi
echo "  2. cd fincept-terminal-desktop"
echo "  3. bun run tauri:dev"
echo ""
echo "Note: If commands not found, restart terminal first!"
echo "========================================"
echo ""
