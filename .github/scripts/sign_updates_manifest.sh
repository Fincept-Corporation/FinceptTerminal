#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# sign_updates_manifest.sh — sign updates.json for the in-app updater and prove
# that installed terminals will accept the signature.
#
#   UPDATE_SIGNING_KEY=<PEM> sign_updates_manifest.sh <manifest> <signature-out>
#   sign_updates_manifest.sh --verify <manifest> <signature>
#
# SIGNING
#   Ed25519 over the manifest bytes exactly as they will be served
#   (`openssl pkeyutl -sign -rawin`; no pre-hash). <signature-out> gets one line:
#   base64 of the 64 raw signature bytes, then a newline (the client trims).
#   The private key is the PEM in $UPDATE_SIGNING_KEY — in CI the repository
#   secret of the same name. It goes to a 0600 file in a private temp directory
#   that is removed on exit, and is never printed. <signature-out> is written
#   only after the signature has verified, so a failed run leaves no .sig.
#
# VERIFICATION (always after signing; on its own with --verify)
#   Against the public key PINNED IN THE CLIENT, not against the private key's
#   own public half: the 64-hex UPDATE_SIGNING_PUBLIC_KEY_HEX in
#   fincept-qt/src/services/updater/UpdateManifest.h of this checkout, turned
#   into a PEM public key (Ed25519 SubjectPublicKeyInfo = the fixed DER prefix
#   302a300506032b6570032100 + the 32 key bytes). A manifest that fails here is
#   one no installed terminal will update from, so every failure exits non-zero
#   with an ::error:: line that says what to do.
#
#   Key rotation (UpdateService.cpp has the runbook): the release that first
#   pins a NEW public key must still be signed with the OLD key, which this
#   check — it trusts this checkout's pin — rejects. Rotating needs a one-off
#   change here; do not weaken the check permanently.
#
# Needs bash, OpenSSL 3.x (pkeyutl -rawin), od, tr.
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

HEADER_REL="fincept-qt/src/services/updater/UpdateManifest.h"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HEADER="${REPO_ROOT}/${HEADER_REL}"
ED25519_SPKI_PREFIX="302a300506032b6570032100"

die() {
    echo "::error::$*"
    exit 1
}

usage() {
    echo "usage: $0 <manifest> <signature-out>   (Ed25519 private key PEM in \$UPDATE_SIGNING_KEY)" >&2
    echo "       $0 --verify <manifest> <signature>" >&2
    exit 2
}

# Hex string (validated by the caller: [0-9a-f], even length) -> raw bytes on
# stdout. Pure bash, so neither xxd nor python is needed.
hex_to_bin() {
    local hex="$1" fmt="" i
    for ((i = 0; i < ${#hex}; i += 2)); do
        fmt+="\\x${hex:i:2}"
    done
    printf "$fmt"
}

bin_to_hex() {
    od -An -v -tx1 "$1" | tr -d ' \t\r\n'
}

file_size() {
    local n
    n=$(wc -c <"$1")
    echo $((n))
}

MODE=sign
if [ "${1:-}" = "--verify" ]; then
    MODE=verify
    shift
fi
[ "$#" -eq 2 ] || usage
MANIFEST="$1"
SIG_PATH="$2"

command -v openssl >/dev/null 2>&1 || die "openssl not found — install OpenSSL 3.x"
case "$(openssl pkeyutl -help 2>&1 || true)" in
    *-rawin*) ;;
    *) die "$(openssl version 2>/dev/null || echo openssl) has no 'pkeyutl -rawin' — OpenSSL 3.x is required to sign/verify Ed25519 over raw bytes" ;;
esac
[ -f "$MANIFEST" ] && [ -s "$MANIFEST" ] || die "manifest '${MANIFEST}' not found or empty"
if [ "$MODE" = verify ]; then
    [ -f "$SIG_PATH" ] || die "signature file '${SIG_PATH}' not found"
else
    # Never leave a stale signature next to a manifest this run failed to sign.
    rm -f "$SIG_PATH"
fi

CALLER_UMASK="$(umask)"
umask 077
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
trap 'exit 130' INT TERM HUP

# ── The public key the client pins ───────────────────────────────────────────
[ -f "$HEADER" ] || die "${HEADER_REL} not found under ${REPO_ROOT} — the pinned public key is read from it (check out the repository, not only .github/scripts)"
PINNED_HITS="$(tr -d '\r' <"$HEADER" \
    | sed -n '/UPDATE_SIGNING_PUBLIC_KEY_HEX[[:space:]]*=/,/;/p' \
    | grep -oE '"[0-9A-Fa-f]{64}"' | tr -d '"' || true)"
if [ -z "$PINNED_HITS" ] || [ "$(printf '%s\n' "$PINNED_HITS" | wc -l)" -ne 1 ]; then
    die "could not read exactly one 64-hex value of UPDATE_SIGNING_PUBLIC_KEY_HEX from ${HEADER_REL} — it must stay a single quoted 64-character hex string (the raw Ed25519 public key)"
fi
PINNED_HEX="$(printf '%s' "$PINNED_HITS" | tr 'A-F' 'a-f')"
hex_to_bin "${ED25519_SPKI_PREFIX}${PINNED_HEX}" >"$TMP/pinned.der"
openssl pkey -pubin -inform DER -in "$TMP/pinned.der" -out "$TMP/pinned.pem" 2>/dev/null \
    || die "UPDATE_SIGNING_PUBLIC_KEY_HEX (${PINNED_HEX}) in ${HEADER_REL} is not a valid Ed25519 public key"

# $1 manifest, $2 signature file (base64). Decodes the FILE, so what is checked
# is exactly what gets published.
verify_signature() {
    local manifest="$1" sig="$2"
    tr -d ' \t\r\n' <"$sig" >"$TMP/verify.b64"
    [ -s "$TMP/verify.b64" ] || die "signature file '${sig}' is empty"
    openssl base64 -d -A -in "$TMP/verify.b64" -out "$TMP/verify.bin" 2>/dev/null \
        || die "signature file '${sig}' is not base64"
    [ "$(file_size "$TMP/verify.bin")" -eq 64 ] \
        || die "signature file '${sig}' does not decode to the 64 bytes of an Ed25519 signature"
    openssl pkeyutl -verify -rawin -pubin -inkey "$TMP/pinned.pem" \
        -in "$manifest" -sigfile "$TMP/verify.bin" >"$TMP/verify.out" 2>&1
}

MANIFEST_SHA="$(openssl dgst -sha256 -r "$MANIFEST" | cut -d' ' -f1)"
MANIFEST_DESC="${MANIFEST} ($(file_size "$MANIFEST") bytes, sha256 ${MANIFEST_SHA})"

if [ "$MODE" = verify ]; then
    if ! verify_signature "$MANIFEST" "$SIG_PATH"; then
        cat "$TMP/verify.out"
        die "${SIG_PATH} is not a valid signature of ${MANIFEST_DESC} under the public key pinned in ${HEADER_REL} (${PINNED_HEX}) — installed terminals would reject this pair. Re-sign the exact manifest bytes with .github/scripts/sign_updates_manifest.sh and publish both files together."
    fi
    echo "OK: ${SIG_PATH} verifies ${MANIFEST_DESC} against the pinned key ${PINNED_HEX}"
    exit 0
fi

# ── Sign ─────────────────────────────────────────────────────────────────────
if [[ "${UPDATE_SIGNING_KEY:-}" =~ ^[[:space:]]*$ ]]; then
    die "UPDATE_SIGNING_KEY is empty or not set: add the repository secret UPDATE_SIGNING_KEY — without it no installed terminal will accept this release as an update. Value: the whole Ed25519 private key PEM file (-----BEGIN PRIVATE KEY----- … -----END PRIVATE KEY-----) whose public half is UPDATE_SIGNING_PUBLIC_KEY_HEX in ${HEADER_REL}; GitHub: Settings → Secrets and variables → Actions → New repository secret. Then re-run this job."
fi
KEY="$TMP/signing.key"
printf '%s\n' "$UPDATE_SIGNING_KEY" >"$KEY"
chmod 600 "$KEY"
unset UPDATE_SIGNING_KEY # child processes do not need it in their environment

# -passin pass: — an encrypted key fails at once instead of prompting.
openssl pkey -in "$KEY" -passin pass: -pubout -outform DER -out "$TMP/key-pub.der" 2>/dev/null \
    || die "UPDATE_SIGNING_KEY is not a readable, unencrypted private key in PEM form — store the whole .key file, including its -----BEGIN PRIVATE KEY----- and -----END PRIVATE KEY----- lines, as the secret value"
KEY_PUB_DER_HEX="$(bin_to_hex "$TMP/key-pub.der")"
if [ "${#KEY_PUB_DER_HEX}" -ne 88 ] || [ "${KEY_PUB_DER_HEX#"$ED25519_SPKI_PREFIX"}" = "$KEY_PUB_DER_HEX" ]; then
    die "UPDATE_SIGNING_KEY is not an Ed25519 key — the updater only accepts Ed25519 signatures; store the Ed25519 private key whose public half is pinned in ${HEADER_REL}"
fi
KEY_PUB_HEX="${KEY_PUB_DER_HEX#"$ED25519_SPKI_PREFIX"}"
if [ "$KEY_PUB_HEX" != "$PINNED_HEX" ]; then
    die "UPDATE_SIGNING_KEY does not match the public key pinned in the client: its public half is ${KEY_PUB_HEX}, but UPDATE_SIGNING_PUBLIC_KEY_HEX in ${HEADER_REL} is ${PINNED_HEX}. Installed terminals only accept manifests signed with the key they pin — set the UPDATE_SIGNING_KEY secret to the private key that belongs to ${PINNED_HEX}, then re-run this job."
fi

openssl pkeyutl -sign -rawin -inkey "$KEY" -passin pass: -in "$MANIFEST" -out "$TMP/sig.bin" 2>"$TMP/sign.err" || {
    cat "$TMP/sign.err"
    die "openssl could not sign ${MANIFEST} (error above)"
}
rm -f "$KEY"
[ "$(file_size "$TMP/sig.bin")" -eq 64 ] || die "openssl produced a signature of $(file_size "$TMP/sig.bin") bytes, expected 64"
SIG_B64="$(openssl base64 -A -in "$TMP/sig.bin" | tr -d '\r\n')"
printf '%s\n' "$SIG_B64" >"$TMP/sig.b64"

if ! verify_signature "$MANIFEST" "$TMP/sig.b64"; then
    cat "$TMP/verify.out"
    die "the signature just made does not verify against the public key pinned in ${HEADER_REL} although the key matches it — do not publish; installed terminals would reject this manifest. Check that ${MANIFEST} was not modified while signing and that openssl works (output above)."
fi

umask "$CALLER_UMASK" # the published .sig is not secret
mkdir -p "$(dirname "$SIG_PATH")"
cp "$TMP/sig.b64" "$SIG_PATH"
echo "Signed ${MANIFEST_DESC}"
echo "  -> ${SIG_PATH} (Ed25519, base64)"
echo "  verifies against UPDATE_SIGNING_PUBLIC_KEY_HEX ${PINNED_HEX} pinned in ${HEADER_REL}"
