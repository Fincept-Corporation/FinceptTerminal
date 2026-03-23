#!/usr/bin/env bash
# Fincept Terminal — Static Analysis / Linting
# Usage:
#   ./scripts/lint.sh              # run clang-format check (fast)
#   ./scripts/lint.sh --fix        # auto-fix formatting with clang-format
#   ./scripts/lint.sh --tidy       # run clang-tidy (deep, slower)
#   ./scripts/lint.sh --cppcheck   # run cppcheck
#   ./scripts/lint.sh --all        # run all checks
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC_DIR="$ROOT_DIR/src"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

FAILED=0

# ── clang-format ──────────────────────────────────────────────────────────────
run_clang_format() {
    local FIX="${1:-check}"
    local CF
    CF=$(command -v clang-format 2>/dev/null || echo "")
    if [[ -z "$CF" ]]; then
        echo -e "${RED}clang-format not found. Install LLVM.${NC}"
        return 1
    fi

    echo -e "${GREEN}Running clang-format ($FIX mode)...${NC}"

    local FILES
    FILES=$(find "$SRC_DIR" -name "*.cpp" -o -name "*.h" | sort)

    if [[ "$FIX" == "fix" ]]; then
        echo "$FILES" | xargs "$CF" -i --style=file
        echo -e "${GREEN}clang-format: files reformatted.${NC}"
    else
        local BAD=()
        while IFS= read -r f; do
            if ! "$CF" --style=file --dry-run --Werror "$f" 2>/dev/null; then
                BAD+=("$f")
            fi
        done <<< "$FILES"

        if [[ ${#BAD[@]} -gt 0 ]]; then
            echo -e "${RED}clang-format: ${#BAD[@]} file(s) need formatting:${NC}"
            printf '  %s\n' "${BAD[@]}"
            echo -e "${YELLOW}Run: ./scripts/lint.sh --fix${NC}"
            FAILED=1
        else
            echo -e "${GREEN}clang-format: all files are correctly formatted.${NC}"
        fi
    fi
}

# ── clang-tidy ────────────────────────────────────────────────────────────────
run_clang_tidy() {
    local CT
    CT=$(command -v clang-tidy 2>/dev/null || echo "")
    if [[ -z "$CT" ]]; then
        echo -e "${RED}clang-tidy not found. Install LLVM.${NC}"
        return 1
    fi

    # Find compile_commands.json
    local COMPILE_DB=""
    for dir in "$ROOT_DIR/build" "$ROOT_DIR/build-msvc" "$ROOT_DIR/build-release" "$ROOT_DIR/build-debug"; do
        if [[ -f "$dir/compile_commands.json" ]]; then
            COMPILE_DB="$dir"
            break
        fi
    done

    if [[ -z "$COMPILE_DB" ]]; then
        echo -e "${YELLOW}No compile_commands.json found. Generate with:${NC}"
        echo -e "${YELLOW}  cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G Ninja -B build${NC}"
        echo -e "${YELLOW}Running clang-tidy without compile DB (limited accuracy)...${NC}"
    fi

    echo -e "${GREEN}Running clang-tidy...${NC}"

    local TIDY_ARGS=("--extra-arg=-std=c++20" "--config-file=$ROOT_DIR/.clang-tidy")
    if [[ -n "$COMPILE_DB" ]]; then
        TIDY_ARGS+=("-p" "$COMPILE_DB")
    fi

    local CPP_FILES
    CPP_FILES=$(find "$SRC_DIR" -name "*.cpp" | grep -v "/moc_" | grep -v "/qrc_" | sort)

    local TIDY_FAILED=0
    echo "$CPP_FILES" | xargs "$CT" "${TIDY_ARGS[@]}" 2>&1 || TIDY_FAILED=1

    if [[ $TIDY_FAILED -ne 0 ]]; then
        echo -e "${RED}clang-tidy: issues found.${NC}"
        FAILED=1
    else
        echo -e "${GREEN}clang-tidy: no issues.${NC}"
    fi
}

# ── cppcheck ──────────────────────────────────────────────────────────────────
run_cppcheck() {
    local CPPCHECK
    CPPCHECK=$(command -v cppcheck 2>/dev/null || echo "")
    if [[ -z "$CPPCHECK" ]]; then
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
        "--suppressions-list=$ROOT_DIR/.cppcheck-suppressions" \
        --suppress=*:*/vendor/* \
        --inline-suppr \
        --std=c++20 \
        --error-exitcode=1 \
        -I "$SRC_DIR" \
        -j "$(nproc 2>/dev/null || echo 4)" \
        "$SRC_DIR" 2>&1 || FAILED=1

    echo -e "${GREEN}cppcheck complete.${NC}"
}

# ── Entry point ───────────────────────────────────────────────────────────────
case "${1:-}" in
    --fix)
        run_clang_format fix
        ;;
    --tidy)
        run_clang_tidy
        ;;
    --cppcheck)
        run_cppcheck
        ;;
    --all)
        run_clang_format check
        echo ""
        run_clang_tidy
        echo ""
        run_cppcheck
        ;;
    *)
        run_clang_format check
        ;;
esac

if [[ $FAILED -ne 0 ]]; then
    echo -e "\n${RED}Lint failed. See above for details.${NC}"
    exit 1
fi
exit 0
