// v025_secure_credentials — pure-SQL credential store backing SecureStorage.
//
// Replaces the OS-native keychain backends (Windows Credential Manager,
// macOS Keychain via SecItem, Linux libsecret/QSettings). Values are
// encrypted at rest with AES-256-GCM; the key is derived from machine-local
// identifiers in SecureStorage.cpp. Schema is intentionally minimal — every
// column is needed to decrypt. `iv` is the AES-GCM nonce (12 bytes) and
// `tag` is the authentication tag (16 bytes); both are stored alongside the
// ciphertext per NIST SP 800-38D.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> v025_sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v025(QSqlDatabase& db) {
    auto r = v025_sql(db, R"sql(
        CREATE TABLE IF NOT EXISTS secure_credentials (
            key        TEXT PRIMARY KEY,
            ciphertext BLOB NOT NULL,
            iv         BLOB NOT NULL,
            tag        BLOB NOT NULL,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )sql");
    return r;
}

} // anonymous namespace

void register_migration_v025() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({25, "secure_credentials", apply_v025});
}

} // namespace fincept
