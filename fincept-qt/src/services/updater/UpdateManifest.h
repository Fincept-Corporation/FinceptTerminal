#pragma once

#include "services/updater/UpdateTypes.h"

#include <QByteArray>
#include <QString>

#include <optional>

// Update manifest: authenticity check, parsing, version comparison.
// Pure functions — no network, no widgets — so tests/tst_update_manifest.cpp
// covers them directly.
//
// Manifest (updates.json, schema-version 3), published as an asset of every
// GitHub release together with its detached signature updates.json.sig:
//
//   {
//     "schema-version": 3,
//     "version": "4.5.4",
//     "release-url": "https://github.com/<owner>/<repo>/releases/tag/v4.5.4",
//     "notes": "short plain text",
//     "assets": {
//       "windows-x64-ifw":    { "url": "https://...", "sha256": "<64 hex>", "size": 123 },
//       "macos-arm64-dmg":    { ... },
//       "linux-x64-deb":      { ... },
//       "linux-x64-flatpak":  { ... }
//     }
//   }
//
// The signature is Ed25519 over the manifest bytes exactly as served, made
// with the key whose public half is pinned below. Nothing in a manifest is
// read before its signature verifies.

namespace fincept::services::updater {

/// Raw Ed25519 public key (32 bytes as 64 hex characters) that update
/// manifests must be signed with. The private half is the release pipeline's
/// UPDATE_SIGNING_KEY secret (.github/workflows/release.yml signs with it and
/// fails the release when the signature does not verify against this value).
inline constexpr const char* UPDATE_SIGNING_PUBLIC_KEY_HEX =
    "b4f20d945ef8588656e77841b2b7b84b8cf90ffb157656949ff0fde7013280e9";

/// True iff `signature` (base64, or 128 hex characters) is a valid Ed25519
/// signature over `manifest` under `public_key_hex`. False for any malformed
/// input — never "no key means no check".
bool verify_manifest_signature(const QByteArray& manifest, const QByteArray& signature,
                               const QString& public_key_hex = QString::fromLatin1(UPDATE_SIGNING_PUBLIC_KEY_HEX));

/// Strict "X.Y.Z" parse. False for anything else (dev builds, suffixes).
bool parse_version(const QString& s, int& major, int& minor, int& patch);

/// True iff `remote` is a well-formed version strictly newer than `local`.
/// False when either side is malformed.
bool is_newer_version(const QString& local, const QString& remote);

struct ManifestResult {
    /// Set when the manifest is well-formed (whether or not it is newer than
    /// the running version). `asset` is filled from assets[asset_key] when
    /// that entry exists and is valid (https URL — or http to a loopback host
    /// when `allow_loopback_http` — and a 64-hex sha256); left empty otherwise.
    std::optional<UpdateInfo> info;
    /// Why `info` is unset; for the log, not shown to the user.
    QString error;
};

/// Parse an ALREADY VERIFIED manifest and pick the asset for `asset_key`
/// (may be empty: no asset is picked).
ManifestResult parse_manifest(const QByteArray& verified_manifest, const QString& asset_key,
                              bool allow_loopback_http = false);

} // namespace fincept::services::updater
