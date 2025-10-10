#!/bin/bash

# Fincept Terminal - Automated Setup Script for Linux/macOS
# This script installs Node.js and Rust, then sets up the project

set -e  # Exit on error

echo "======================================="
echo "  Fincept Terminal Setup"
echo "  Automated installer for Linux/macOS"
echo "======================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to print colored output
print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}→ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# Check if Node.js is installed
print_info "Checking for Node.js..."
if command -v node &> /dev/null; then
    NODE_VERSION=$(node -v)
    print_success "Node.js is already installed: $NODE_VERSION"
else
    print_info "Node.js not found. Installing Node.js LTS..."

    # Detect OS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS - use Homebrew
        if ! command -v brew &> /dev/null; then
            print_info "Installing Homebrew first..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        brew install node
    else
        # Linux - use NodeSource
        print_info "Downloading Node.js LTS (v22.x) via NodeSource..."
        curl -fsSL https://deb.nodesource.com/setup_lts.x | sudo -E bash -
        sudo apt-get install -y nodejs
    fi

    print_success "Node.js installed successfully!"
fi

# Check if Rust is installed
print_info "Checking for Rust..."
if command -v rustc &> /dev/null; then
    RUST_VERSION=$(rustc --version)
    print_success "Rust is already installed: $RUST_VERSION"
else
    print_info "Rust not found. Installing Rust via rustup..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y

    # Source cargo env
    source "$HOME/.cargo/env"

    print_success "Rust installed successfully!"
fi

# Navigate to project directory
print_info "Navigating to project directory..."
cd "$(dirname "$0")/fincept-terminal-desktop" || {
    print_error "Failed to find fincept-terminal-desktop directory"
    exit 1
}

# Install npm dependencies
print_info "Installing npm dependencies..."
npm install

print_success "Dependencies installed successfully!"

echo ""
echo "======================================="
echo -e "${GREEN}✓ Setup Complete!${NC}"
echo "======================================="
echo ""
echo "To start development:"
echo "  cd fincept-terminal-desktop"
echo "  npm run tauri dev"
echo ""
echo "To build for production:"
echo "  npm run tauri build"
echo ""
