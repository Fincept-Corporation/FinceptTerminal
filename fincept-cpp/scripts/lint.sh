#!/usr/bin/env bash
# Fincept Terminal — Static Analysis / Linting
# Usage:
#   ./scripts/lint.sh              # run cppcheck (fast)
#   ./scripts/lint.sh --tidy       # run clang-tidy (deep, slower)
#   ./scripts/lint.sh --all        # run both
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC_DIR="$ROOT_DIR/src"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

run_cppcheck() {
    local CPPCHECK
    CPPCHECK=$(command -v cppcheck 2>/dev/null || echo "")
    if [[ -z "$CPPCHECK" ]]; then
        # Windows fallback
        if [[ -x "/c/Program Files/Cppcheck/cppcheck.exe" ]]; then
            CPPCHECK="/c/Program Files/Cppcheck/cppcheck.exe"
        else
            echo -e "${RED}cppcheck not found. Install: winget install Cppcheck.Cppcheck${NC}"
            return 1
        fi
    fi

    echo -e "${GREEN}Running cppcheck...${NC}"
    "$CPPCHECK" \
        --enable=warning,performance,portability \
        --suppress=missingIncludeSystem \
        --suppress=unusedFunction \
        --suppress=*:*/vendor/* \
        --inline-suppr \
        --std=c++20 \
        -I "$SRC_DIR" \
        -j "$(nproc 2>/dev/null || echo 4)" \
        "$SRC_DIR" 2>&1

    echo -e "${GREEN}cppcheck complete.${NC}"
}

run_clang_tidy() {
    local CLANG_TIDY
    CLANG_TIDY=$(command -v clang-tidy 2>/dev/null || echo "")
    if [[ -z "$CLANG_TIDY" ]]; then
        echo -e "${RED}clang-tidy not found. Install LLVM.${NC}"
        return 1
    fi

    # Find compile_commands.json
    local COMPILE_DB=""
    for dir in "$ROOT_DIR/build" "$ROOT_DIR/build-msvc" "$ROOT_DIR/build-release"; do
        if [[ -f "$dir/compile_commands.json" ]]; then
            COMPILE_DB="$dir"
            break
        fi
    done

    if [[ -z "$COMPILE_DB" ]]; then
        echo -e "${YELLOW}No compile_commands.json found. Running clang-tidy without it (limited).${NC}"
        echo -e "${YELLOW}Generate with: cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G Ninja -B build${NC}"
    fi

    echo -e "${GREEN}Running clang-tidy...${NC}"

    local TIDY_ARGS=("--extra-arg=-std=c++20")
    if [[ -n "$COMPILE_DB" ]]; then
        TIDY_ARGS+=("-p" "$COMPILE_DB")
    fi

    # Run on key source files (not the entire tree — too slow)
    find "$SRC_DIR/trading" "$SRC_DIR/screens/crypto_trading" "$SRC_DIR/core" \
        -name "*.cpp" -print0 2>/dev/null | \
        xargs -0 "$CLANG_TIDY" "${TIDY_ARGS[@]}" 2>&1

    echo -e "${GREEN}clang-tidy complete.${NC}"
}

case "${1:-}" in
    --tidy)
        run_clang_tidy
        ;;
    --all)
        run_cppcheck
        echo ""
        run_clang_tidy
        ;;
    *)
        run_cppcheck
        ;;
esac
