#!/usr/bin/env bash
# macos_codesign.sh — shared signing helpers for the macOS release job.
#
# SOURCE this file, don't execute it:
#     source "$GITHUB_WORKSPACE/.github/scripts/macos_codesign.sh"
#
# WHY THIS EXISTS
#
# Every `codesign --sign` call in release.yml runs under `set -e` and is
# unguarded, so a single failure ends the release job — roughly 35 minutes in,
# after the build, the self-tests, the bundle staging and macdeployqt have all
# succeeded. Two failure modes account for essentially all of those:
#
#   1. Apple's timestamp service (timestamp.apple.com) rate-limits or 5xx's.
#      `--timestamp` is mandatory for notarization, and codesign treats a TSA
#      hiccup as a hard error. It is transient and retryable, but nothing
#      retried it, so the job died and the release never published.
#
#   2. The Developer ID identity is not actually usable — expired cert,
#      rotated MACOS_SIGNING_IDENTITY secret, a p12 that imported without its
#      private key. The import step LOOKED fine because
#      `security find-identity -v -p codesigning` prints "0 valid identities
#      found" and still EXITS 0, so the first real failure surfaced several
#      steps later as an opaque "Process completed with exit code 1".
#
# Both are addressed here: assert_signing_identity turns (2) into a named
# error at import time, and codesign_retry absorbs (1).

# assert_signing_identity <keychain-path> <signing-identity>
#
# Fails loudly when the keychain holds no usable codesigning identity, or when
# the identity the workflow is about to sign with isn't among them. Never
# echoes the identity itself (it is a repository secret; Actions masks it, but
# there is no reason to push it through an extra pipe).
assert_signing_identity() {
    local keychain="$1"
    local identity="$2"
    local listing count

    if [ -z "${identity}" ]; then
        echo "::error::MACOS_SIGNING_IDENTITY is empty. Set the repository secret to the full" \
             "certificate name, e.g. 'Developer ID Application: Your Org (TEAMID)', or its SHA-1 hash." >&2
        return 1
    fi

    listing="$(security find-identity -v -p codesigning "${keychain}" 2>&1 || true)"
    echo "${listing}"

    # `find-identity -v` lists only valid identities — an expired or untrusted
    # certificate simply doesn't appear, so a zero count covers expiry too.
    count="$(printf '%s\n' "${listing}" | grep -cE '^[[:space:]]*[0-9]+\)' || true)"
    if [ "${count}" -eq 0 ]; then
        echo "::error::No valid codesigning identity in the build keychain. The Developer ID" \
             "certificate in MACOS_CERT_P12_BASE64 is missing, expired, untrusted, or was exported" \
             "without its private key. Re-export it from Keychain Access as a .p12 INCLUDING the" \
             "private key, base64 it, and update MACOS_CERT_P12_BASE64 + MACOS_CERT_PASSWORD." >&2
        return 1
    fi

    if ! printf '%s\n' "${listing}" | grep -qF -- "${identity}"; then
        echo "::error::MACOS_SIGNING_IDENTITY does not match any identity in the keychain" \
             "(${count} valid identit(y/ies) present — see the list above). The secret and the" \
             "certificate have drifted apart; copy the exact name from the listing." >&2
        return 1
    fi

    echo "Signing identity resolved (${count} valid identit(y/ies) in keychain)."
}

# codesign_retry <codesign args...>
#
# codesign with bounded retries and exponential backoff. Retries every failure
# rather than pattern-matching the timestamp error: codesign's wording varies
# across Xcode versions ("The timestamp service is not available", "Timestamp
# service did not respond", plain -1 from cssm), and a genuinely broken input
# just fails five times a few seconds apart and then reports normally. The
# final attempt's stderr is printed verbatim so the log still names the file
# and the real reason.
codesign_retry() {
    local attempt=1
    local max="${CODESIGN_MAX_ATTEMPTS:-5}"
    local delay="${CODESIGN_RETRY_DELAY:-5}"
    local out rc target

    # Last positional arg = the path being signed, for the error message.
    # Kept in a variable rather than read as ${*: -1} because the runners'
    # /bin/bash is 3.2 and this must not depend on newer expansions.
    for target in "$@"; do :; done

    while :; do
        # `x=$(cmd) && rc=0 || rc=$?` instead of toggling `set +e`/`set -e`:
        # `set` is process-global, not function-local (and `local -` needs
        # bash 4.4, newer than the runners' 3.2), so toggling it here would
        # leak errexit back to a caller that had deliberately disabled it.
        out="$(codesign "$@" 2>&1)" && rc=0 || rc=$?

        if [ "${rc}" -eq 0 ]; then
            if [ -n "${out}" ]; then
                printf '%s\n' "${out}"
            fi
            return 0
        fi

        if [ "${attempt}" -ge "${max}" ]; then
            printf '%s\n' "${out}" >&2
            # stderr, not stdout: two call sites send this command's stdout to
            # /dev/null, and an annotation nobody sees is worse than none.
            echo "::error::codesign failed after ${max} attempts (last exit ${rc}). Target: ${target}" >&2
            return "${rc}"
        fi

        echo "codesign attempt ${attempt}/${max} failed (exit ${rc}), retrying in ${delay}s: ${out}" >&2
        sleep "${delay}"
        attempt=$((attempt + 1))
        delay=$((delay * 2))
    done
}
