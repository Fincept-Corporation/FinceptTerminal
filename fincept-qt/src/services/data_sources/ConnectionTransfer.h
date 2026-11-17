#pragma once
// ConnectionTransfer — the file format for exporting and importing Data Sources connections.
//
// Secrets (passwords, API keys, tokens, custom auth headers) are EXCLUDED unless the user opts
// in; when included they are encrypted with a passphrase the user chooses:
//   key  = PBKDF2-HMAC-SHA256(passphrase, 16-byte random salt, 310 000 iterations) → 32 bytes
//   each connection's secrets = AES-256-GCM(key, 12-byte random IV, AAD "fincept.connections.v2")
// The file never holds a secret in plain text. A "check" block (a known text encrypted the same
// way) lets import reject a wrong passphrase before anything is written.
//
// Import also reads the version-1 file (a plain JSON array written by earlier versions; its
// config may carry plaintext secrets, which are taken as they are).
//
// Pure apart from OpenSSL (QtCore + libcrypto): no widgets, no database — callers run it on a
// worker thread (PBKDF2 is deliberately slow). Tested in tests/tst_connection_transfer.cpp.

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::datasources {

struct TransferConnection {
    QString id;
    QString alias;
    QString display_name;
    QString description;
    QString type;
    QString provider;
    QString category;
    QString tags;
    bool enabled = true;
    QJsonObject config;           ///< complete config (secrets included when known)
    QStringList secret_fields;    ///< export: fields the connector declares secret (Password fields)
};

/// The secret-field rule (same as DataSourceRepository's): password / secret / token / api key /
/// private / access / account key / credential / connection string, Cosmos "key", custom "headers".
bool is_secret_config_key(const QString& key);

struct ExportOptions {
    bool include_secrets = false;
    QString passphrase;           ///< required (≥ 8 characters) when include_secrets
    int iterations = 310000;
};

struct ExportResult {
    bool ok = false;
    QString error;
    QByteArray json;
    int connections = 0;
    int secrets_excluded = 0;     ///< secret values left out of the file
    int secrets_encrypted = 0;    ///< secret values written encrypted
};

ExportResult export_connections_document(const QVector<TransferConnection>& connections, const ExportOptions& options);

struct ImportedConnection {
    TransferConnection connection; ///< config with the decrypted secrets merged in
    QStringList missing_secrets;   ///< secret fields the file did not carry (re-enter after import)
};

struct ImportResult {
    bool ok = false;
    QString error;
    int format_version = 0;
    bool has_encrypted_secrets = false;
    bool needs_passphrase = false;  ///< encrypted secrets and no passphrase given: ask, then read again
    bool wrong_passphrase = false;
    QVector<ImportedConnection> connections;
    int secrets_restored = 0;
    int secrets_missing = 0;
    int skipped = 0;                ///< entries without a provider
};

/// Parse an export file. `passphrase` may be empty: encrypted secrets are then reported as
/// needs_passphrase (and nothing else is returned) unless `skip_secrets` is set, in which case
/// the connections come back without them (listed in missing_secrets).
ImportResult read_connections_document(const QByteArray& bytes, const QString& passphrase, bool skip_secrets = false);

// ── Primitives (exposed for tests) ─────────────────────────────────────────
QByteArray transfer_derive_key(const QString& passphrase, const QByteArray& salt, int iterations);
/// AES-256-GCM; returns {iv, tag, data} as base64 in a JSON object, or empty on failure.
QJsonObject transfer_encrypt(const QByteArray& key, const QByteArray& plain);
/// Inverse; false when the tag does not verify (wrong key or damaged data).
bool transfer_decrypt(const QByteArray& key, const QJsonObject& box, QByteArray* plain);

} // namespace fincept::services::datasources
