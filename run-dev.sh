#!/usr/bin/env bash
# Launch AdamTerminal (Fincept Terminal) in DEV mode.
#
# FINCEPT_DEV_NO_LOGIN=1 makes AuthManager inject an in-memory "guest"
# enterprise session and the shell skip the login + PIN gate, opening straight
# to the dashboard. This is a local-development convenience only — never ship
# a build expecting this variable, and unset it to test the real auth flow.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$ROOT/fincept-qt/build/macos-release/FinceptTerminal.app/Contents/MacOS/FinceptTerminal"

if [ ! -x "$BIN" ]; then
    echo "Binary not found at: $BIN" >&2
    echo "Build first:  cd fincept-qt && \\"  >&2
    echo "  MAMBA_ROOT_PREFIX=$ROOT/.mamba $ROOT/bin/micromamba run -n fincept \\" >&2
    echo "  cmake --build --preset macos-release --parallel 3" >&2
    exit 1
fi

echo "Launching AdamTerminal (DEV — no login/PIN) ..."
FINCEPT_DEV_NO_LOGIN=1 exec "$BIN" "$@"
