#!/bin/bash
# Fincept Terminal C++ Build Script (macOS/Linux)
# Prerequisites: CMake, Ninja, vcpkg

set -e

echo "================================================"
echo " Fincept Terminal C++ Build"
echo "================================================"

# Check vcpkg
if [ -z "$VCPKG_ROOT" ]; then
    echo "ERROR: VCPKG_ROOT is not set"
    echo "Please set VCPKG_ROOT to your vcpkg installation directory"
    echo "Example: export VCPKG_ROOT=\$HOME/vcpkg"
    exit 1
fi

echo "Using vcpkg at: $VCPKG_ROOT"

# Detect triplet
if [ "$(uname)" = "Darwin" ]; then
    TRIPLET="x64-osx"
else
    TRIPLET="x64-linux"
fi

# Install dependencies
echo ""
echo "[1/3] Installing dependencies via vcpkg..."
$VCPKG_ROOT/vcpkg install --triplet $TRIPLET

# Configure
echo ""
echo "[2/3] Configuring with CMake..."
cmake --preset default

# Build
echo ""
echo "[3/3] Building..."
cmake --build build

echo ""
echo "================================================"
echo " Build Complete!"
echo " Run: ./build/FinceptTerminal"
echo "================================================"
