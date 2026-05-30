#!/bin/bash
# Run clang-tidy-18 on changed C++ source files using flags extracted from the
# unity-build compile DB (so unity-included files can still be analyzed).
# Usage: run-tidy-changed.sh [file1 file2 ...]   (no args = all changed .cpp)
set -uo pipefail

REPO=/Users/tilak/projects/FinceptTerminal
TIDY=/opt/homebrew/opt/llvm@18/bin/clang-tidy
CFG=/tmp/fincept-tidy18.yaml
SDK=$(xcrun --show-sdk-path)

cd "$REPO/fincept-qt" || exit 2

# Extract the target's compile flags from a unity blob entry (identical for all
# files in the FinceptTerminal target). Drop compiler, -c/-o, and source file.
FLAGS=()
while IFS= read -r line; do [ -n "$line" ] && FLAGS+=("$line"); done < <(python3 - <<'PY'
import json
db=json.load(open("build/macos-release/compile_commands.json"))
e=next(x for x in db if "Unity/unity_" in x["file"])
toks=(e.get("command") or " ".join(e["arguments"])).split()
out=[]; i=1
while i < len(toks):
    t=toks[i]
    if t in ("-c","-o"): i+=2; continue
    # Drop CMake precompiled-header injection (PCH not built for lint)
    # Strip the whole CMake PCH group: -Winvalid-pch -Xclang -include-pch
    # -Xclang <pch> -Xclang -include -Xclang <hxx>  (real includes use -I)
    if t in ("-Xclang","-include-pch","-include","-Winvalid-pch"): i+=1; continue
    if "cmake_pch" in t: i+=1; continue
    out.append(t); i+=1
out=[t for t in out if not t.endswith(".cxx") and not t.endswith(".o")]
print("\n".join(out))
PY
)
FLAGS+=("-isysroot" "$SDK")

FILES=()
if [ "$#" -gt 0 ]; then
  FILES=("$@")
else
  while IFS= read -r line; do [ -n "$line" ] && FILES+=("$line"); done < <(
    git -C "$REPO" diff --name-only --diff-filter=d \
      | grep -E 'fincept-qt/src/.*\.cpp$' | grep -vE '/moc_|/qrc_' \
      | sed 's#fincept-qt/##'
  )
fi

echo "clang-tidy-18 on ${#FILES[@]} file(s)  |  ${#FLAGS[@]} compile flags"
echo "=================================================="
for f in "${FILES[@]}"; do
  [ -f "$f" ] || { echo "MISSING FILE: $f"; continue; }
  "$TIDY" --config-file="$CFG" --quiet "$f" -- "${FLAGS[@]}" 2>&1
done
