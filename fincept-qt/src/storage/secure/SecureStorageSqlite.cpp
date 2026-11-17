// SecureStorageSqlite — the production ISecretBackend: SQLite + AES-256-GCM
// (moved here from SecureStorage.cpp, which is now the facade).
//
// Values live in `secure_credentials` (created by migration v029) encrypted
// with AES-256-GCM under a per-install random *data encryption key* (DEK).
// The DEK itself is stored in the same table under a reserved key, wrapped by
// a platform key-encryption key (KEK):
//
//   Windows  — DPAPI (CryptProtectData, user scope). The wrapping key lives in
//              the user's Windows profile and is protected by their logon
//              credentials: the DB file alone is useless, and another local
//              user cannot decrypt it.
//   Other    — PBKDF2-HMAC-SHA256 over the machine-local seed with a random
//              per-install salt. The seed (machine ID) is readable by any local
//              process, so this is a speed bump against offline DB theft, not a
//              defence against a local attacker. macOS Keychain / libsecret are
//              not wired in (no Keychain / libsecret code exists in this tree).
//
// Reads are TRI-STATE (SecretRead.h). The defect this file used to have: a
// wrapped-key row that could not be read (database busy) or unwrapped (DPAPI
// refused) was treated as "no key yet", a NEW key was created and written over
// the old wrapped key — every saved credential became unreadable for good, and
// each read of one then said "Not found", so callers re-ran their setup. A new
// key is now created only when both wrapped-key rows were read successfully and
// neither exists (secure_detail::decide_dek); everything else is an error the
// caller sees, and a failed key load is retried (not cached for the session).
//
// Migration is automatic and lossless. Installs predating the DEK derived the
// row key directly as SHA-256(machine seed); on first access we derive that
// legacy key, re-encrypt every row under a fresh DEK, and store the wrapped
// DEK — inside one transaction, so a crash mid-migration rolls back.

#include "storage/secure/SecureStorage.h"

#include "core/logging/Logger.h"
#include "storage/secure/SecretMigration.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/SqliteSupport.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSysInfo>
#include <QVector>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#ifdef Q_OS_WIN
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
// dpapi.h must follow windows.h — it depends on its typedefs.
#    include <dpapi.h>
#endif

namespace fincept {

namespace secure_sqlite_detail {

constexpr const char* kSecureSqliteTag = "SecureStorage";

constexpr int kKeyLen = 32;  // AES-256
constexpr int kIvLen = 12;   // GCM-recommended nonce size (RFC 5116 §5)
constexpr int kTagLen = 16;  // GCM authentication tag (128 bits)
constexpr int kSaltLen = 16; // PBKDF2 salt
constexpr int kPbkdf2Iterations = 200000;

// A failed key load is retried by the next caller after this long — never
// cached for the whole session (one busy moment at start-up used to make every
// credential "missing" until restart).
constexpr qint64 kKeyRetryAfterMs = 1000;

// Stable per-app salt mixed into the machine-ID hash so a different app
// on the same machine derives a different key. Bumping this rotates
// every key — only do that with a fresh DB.
constexpr const char* kAppSalt = "fincept-terminal/secure-storage/v1";

// Reserved rows holding the wrapped DEK. Never returned to callers.
constexpr const char* kDekPrefix = "__fincept_dek_v2";
constexpr const char* kDekDpapi = "__fincept_dek_v2_dpapi";
constexpr const char* kDekPbkdf2 = "__fincept_dek_v2_pbkdf2";

bool ss_is_reserved_key(const QString& key) {
    return key.startsWith(QLatin1String(kDekPrefix));
}

QByteArray ss_random_bytes(int n) {
    QByteArray out(n, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(out.data()), n) != 1) {
        LOG_ERROR(kSecureSqliteTag, logf("RAND_bytes failed, refusing to produce weak key material",
                                         {{"bytes", n}, {"error", QString::number(ERR_get_error(), 16)}}));
        return {};
    }
    return out;
}

// The machine-local seed: the platform machine ID (per-user on macOS,
// per-machine on Windows/Linux), the OS product type and the app salt.
QByteArray ss_machine_seed() {
    QByteArray seed;
    seed.append(QSysInfo::machineUniqueId());
    seed.append('\x1f');
    seed.append(QSysInfo::productType().toUtf8());
    seed.append('\x1f');
    seed.append(kAppSalt);
    return seed;
}

// Pre-DEK key derivation. Only used to read rows written by older installs.
QByteArray ss_legacy_key() {
    return QCryptographicHash::hash(ss_machine_seed(), QCryptographicHash::Sha256);
}

/// SQLite failure → SecretRead error kind.
SecretRead::Error ss_kind_of(const QSqlError& e) {
    switch (db::classify(e)) {
        case db::SqlErrorKind::Busy:
        case db::SqlErrorKind::Locked:
            return SecretRead::Error::Busy;
        case db::SqlErrorKind::Corrupt:
        case db::SqlErrorKind::NotADb:
            return SecretRead::Error::Corrupt;
        default:
            return SecretRead::Error::Io;
    }
}

// AES-256-GCM encrypt. Returns ciphertext + the GCM tag through `tag_out`.
// On any OpenSSL error returns an empty QByteArray and logs — callers must
// check both isEmpty() AND tag_out.size() == kTagLen before persisting.
QByteArray ss_gcm_encrypt(const QByteArray& key, const QByteArray& iv, const QByteArray& plaintext,
                          QByteArray& tag_out) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return {};

    QByteArray out;
    bool ok = false;
    do {
        if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr))
            break;
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIvLen, nullptr))
            break;
        if (1 != EVP_EncryptInit_ex(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.constData()),
                                    reinterpret_cast<const unsigned char*>(iv.constData())))
            break;
        // GCM is a stream mode: output length equals input length; +16 is margin.
        out.resize(plaintext.size() + 16);
        int len = 0;
        if (1 != EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &len,
                                   reinterpret_cast<const unsigned char*>(plaintext.constData()),
                                   static_cast<int>(plaintext.size())))
            break;
        int written = len;
        if (1 != EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + written, &len))
            break;
        written += len;
        out.resize(written);
        tag_out.resize(kTagLen);
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen, tag_out.data()))
            break;
        ok = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        LOG_ERROR(kSecureSqliteTag, QString("AES-GCM encrypt failed: OpenSSL err 0x%1").arg(ERR_get_error(), 0, 16));
        tag_out.clear();
        return {};
    }
    return out;
}

// AES-256-GCM decrypt with tag verification. `ok_out` is false on a tag
// mismatch (damaged / tampered row, other machine) — an empty QByteArray is
// also a valid plaintext, so callers check ok_out, never the bytes.
QByteArray ss_gcm_decrypt(const QByteArray& key, const QByteArray& iv, const QByteArray& ciphertext,
                          const QByteArray& tag, bool& ok_out) {
    ok_out = false;
    if (key.size() != kKeyLen || iv.size() != kIvLen || tag.size() != kTagLen)
        return {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return {};

    QByteArray out;
    do {
        if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr))
            break;
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIvLen, nullptr))
            break;
        if (1 != EVP_DecryptInit_ex(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.constData()),
                                    reinterpret_cast<const unsigned char*>(iv.constData())))
            break;
        out.resize(ciphertext.size() + 16);
        int len = 0;
        if (1 != EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &len,
                                   reinterpret_cast<const unsigned char*>(ciphertext.constData()),
                                   static_cast<int>(ciphertext.size())))
            break;
        int written = len;
        // EVP_CTRL_GCM_SET_TAG must precede EVP_DecryptFinal_ex, which verifies it.
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen, const_cast<char*>(tag.constData())))
            break;
        if (1 != EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + written, &len))
            break; // tag mismatch
        written += len;
        out.resize(written);
        ok_out = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    return out;
}

// ── DEK wrapping ────────────────────────────────────────────────────────────
// A wrapped DEK is stored in the (ciphertext, iv, tag) columns of its reserved
// row. The row key names the scheme so a reader knows how to read the blobs.

#ifdef Q_OS_WIN
// DPAPI: ciphertext = the opaque DPAPI blob, iv/tag unused (zero bytes).
bool ss_dpapi_wrap(const QByteArray& dek, QByteArray& blob_out) {
    QByteArray entropy(kAppSalt);
    DATA_BLOB in{static_cast<DWORD>(dek.size()), reinterpret_cast<BYTE*>(const_cast<char*>(dek.constData()))};
    DATA_BLOB ent{static_cast<DWORD>(entropy.size()), reinterpret_cast<BYTE*>(const_cast<char*>(entropy.constData()))};
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"Fincept Terminal credential key", &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                          &out)) {
        LOG_WARN(kSecureSqliteTag, QString("CryptProtectData failed error=%1").arg(GetLastError()));
        return false;
    }
    blob_out = QByteArray(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return true;
}

bool ss_dpapi_unwrap(const QByteArray& blob, QByteArray& dek_out) {
    QByteArray entropy(kAppSalt);
    DATA_BLOB in{static_cast<DWORD>(blob.size()), reinterpret_cast<BYTE*>(const_cast<char*>(blob.constData()))};
    DATA_BLOB ent{static_cast<DWORD>(entropy.size()), reinterpret_cast<BYTE*>(const_cast<char*>(entropy.constData()))};
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        LOG_WARN(kSecureSqliteTag, QString("CryptUnprotectData failed error=%1").arg(GetLastError()));
        return false;
    }
    dek_out = QByteArray(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return true;
}
#endif

// PBKDF2 KEK over the machine seed.
QByteArray ss_pbkdf2_kek(const QByteArray& salt) {
    const QByteArray seed = ss_machine_seed();
    QByteArray kek(kKeyLen, '\0');
    if (1 != PKCS5_PBKDF2_HMAC(seed.constData(), static_cast<int>(seed.size()),
                               reinterpret_cast<const unsigned char*>(salt.constData()), static_cast<int>(salt.size()),
                               kPbkdf2Iterations, EVP_sha256(), kKeyLen, reinterpret_cast<unsigned char*>(kek.data()))) {
        LOG_ERROR(kSecureSqliteTag, logf("PBKDF2 derivation failed", {{"error", QString::number(ERR_get_error(), 16)}}));
        return {};
    }
    return kek;
}

struct WrappedRow {
    QString key;
    QByteArray ciphertext;
    QByteArray iv;
    QByteArray tag;
};

// Placeholders for schemes that carry their own integrity (DPAPI) and so have
// no separate iv/tag. The columns are NOT NULL and a default-constructed
// QByteArray binds as SQL NULL, which fails the insert — always bind real bytes.
// [[maybe_unused]]: callers are Windows-only (-Wunused-function is -Werror on CI).
[[maybe_unused]] QByteArray ss_unused_iv() {
    return QByteArray(kIvLen, '\0');
}
[[maybe_unused]] QByteArray ss_unused_tag() {
    return QByteArray(kTagLen, '\0');
}

// PBKDF2 scheme: ciphertext = salt(16) || wrapped_dek, iv = GCM nonce, tag = GCM tag.
bool ss_pbkdf2_wrap(const QByteArray& dek, WrappedRow& out) {
    const QByteArray salt = ss_random_bytes(kSaltLen);
    const QByteArray iv = ss_random_bytes(kIvLen);
    if (salt.isEmpty() || iv.isEmpty())
        return false;
    const QByteArray kek = ss_pbkdf2_kek(salt);
    if (kek.size() != kKeyLen)
        return false;
    QByteArray tag;
    const QByteArray wrapped = ss_gcm_encrypt(kek, iv, dek, tag);
    if (tag.size() != kTagLen)
        return false;
    out.key = QString::fromLatin1(kDekPbkdf2);
    out.ciphertext = salt + wrapped;
    out.iv = iv;
    out.tag = tag;
    return true;
}

bool ss_pbkdf2_unwrap(const WrappedRow& row, QByteArray& dek_out) {
    if (row.ciphertext.size() <= kSaltLen)
        return false;
    const QByteArray salt = row.ciphertext.left(kSaltLen);
    const QByteArray wrapped = row.ciphertext.mid(kSaltLen);
    const QByteArray kek = ss_pbkdf2_kek(salt);
    if (kek.size() != kKeyLen)
        return false;
    bool ok = false;
    dek_out = ss_gcm_decrypt(kek, row.iv, wrapped, row.tag, ok);
    return ok && dek_out.size() == kKeyLen;
}

// ── DEK load / create / migrate ─────────────────────────────────────────────

/// Tri-state: the row, no row, or the read failed (`err_out`).
secure_detail::RowState ss_read_wrapped_row(QSqlDatabase& conn, const char* key, WrappedRow& out, QSqlError* err_out) {
    QSqlQuery q(conn);
    q.prepare(QStringLiteral("SELECT ciphertext, iv, tag FROM secure_credentials WHERE key = ?"));
    q.addBindValue(QString::fromLatin1(key));
    if (!q.exec()) {
        if (err_out)
            *err_out = q.lastError();
        return secure_detail::RowState::ReadError;
    }
    if (!q.next())
        return secure_detail::RowState::Absent;
    out.key = QString::fromLatin1(key);
    out.ciphertext = q.value(0).toByteArray();
    out.iv = q.value(1).toByteArray();
    out.tag = q.value(2).toByteArray();
    return secure_detail::RowState::Present;
}

bool ss_write_wrapped_row(QSqlDatabase& conn, const WrappedRow& row) {
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(R"sql(
        INSERT INTO secure_credentials (key, ciphertext, iv, tag, updated_at)
        VALUES (?, ?, ?, ?, datetime('now'))
        ON CONFLICT(key) DO UPDATE SET
            ciphertext = excluded.ciphertext,
            iv         = excluded.iv,
            tag        = excluded.tag,
            updated_at = excluded.updated_at
    )sql"));
    q.addBindValue(row.key);
    q.addBindValue(row.ciphertext);
    q.addBindValue(row.iv);
    q.addBindValue(row.tag);
    if (!q.exec()) {
        LOG_ERROR(kSecureSqliteTag, QString("Wrapped key could not be written error=\"%1\"").arg(q.lastError().text()));
        return false;
    }
    return true;
}

[[maybe_unused]] bool ss_drop_row(QSqlDatabase& conn, const char* key, QString* error = nullptr) {
    QSqlQuery q(conn);
    q.prepare(QStringLiteral("DELETE FROM secure_credentials WHERE key = ?"));
    q.addBindValue(QString::fromLatin1(key));
    const bool ok = q.exec();
    if (!ok && error)
        *error = q.lastError().text();
    return ok;
}

#ifdef Q_OS_WIN
// Re-wrap an existing DEK under DPAPI and retire the PBKDF2 copy. Best-effort —
// on failure the PBKDF2 row stays authoritative and we retry next launch. The
// new row is written BEFORE the old one is dropped, so an interrupt can never
// leave the DEK unrecoverable.
void ss_upgrade_wrapping_to_dpapi(QSqlDatabase& conn, const QByteArray& dek) {
    QByteArray blob;
    if (!ss_dpapi_wrap(dek, blob))
        return;
    if (!ss_write_wrapped_row(conn, {QString::fromLatin1(kDekDpapi), blob, ss_unused_iv(), ss_unused_tag()}))
        return;
    // Write -> read back -> compare -> only then drop the old wrapping: the new
    // row must unwrap, here, to this very key before the PBKDF2 copy goes.
    WrappedRow back;
    QByteArray back_dek;
    const bool confirmed = ss_read_wrapped_row(conn, kDekDpapi, back, nullptr) == secure_detail::RowState::Present &&
                           ss_dpapi_unwrap(back.ciphertext, back_dek) && back_dek.size() == kKeyLen;
    if (!secret_migration::may_drop_old_key_wrapping(confirmed, back_dek, true, dek)) {
        // The PBKDF2 row stays authoritative; an unconfirmed DPAPI row must not
        // take precedence at the next start. Retried next launch.
        (void)ss_drop_row(conn, kDekDpapi);
        LOG_WARN(kSecureSqliteTag, "DPAPI-wrapped key could not be confirmed after writing; the PBKDF2 copy is kept");
        return;
    }
    QString drop_error;
    if (!ss_drop_row(conn, kDekPbkdf2, &drop_error)) {
        LOG_WARN(kSecureSqliteTag, logf("Wrote the DPAPI-wrapped key but could not drop the PBKDF2 row",
                                        {{"error", drop_error.left(200)}}));
        return;
    }
    LOG_INFO(kSecureSqliteTag, "Upgraded credential key wrapping from PBKDF2 to DPAPI");
}
#endif

// Re-encrypt every credential row from `from_key` to `to_key`. Rows that fail
// to decrypt are left untouched (already unreadable; destroying them loses
// nothing but the evidence).
bool ss_reencrypt_rows(QSqlDatabase& conn, const QByteArray& from_key, const QByteArray& to_key, int& migrated_out) {
    QVector<WrappedRow> rows;
    {
        QSqlQuery sel(conn);
        if (!sel.exec(QStringLiteral("SELECT key, ciphertext, iv, tag FROM secure_credentials"))) {
            LOG_ERROR(kSecureSqliteTag, QString("Migration select failed error=\"%1\"").arg(sel.lastError().text()));
            return false;
        }
        while (sel.next()) {
            const QString k = sel.value(0).toString();
            if (ss_is_reserved_key(k))
                continue;
            rows.push_back({k, sel.value(1).toByteArray(), sel.value(2).toByteArray(), sel.value(3).toByteArray()});
        }
    }
    migrated_out = 0;
    for (const WrappedRow& r : rows) {
        bool ok = false;
        const QByteArray plaintext = ss_gcm_decrypt(from_key, r.iv, r.ciphertext, r.tag, ok);
        if (!ok) {
            // Left as it is (already unreadable); the credential's name only.
            LOG_WARN(kSecureSqliteTag, logf("Legacy credential row could not be migrated and stays unreadable",
                                            {{"key", r.key}}));
            continue;
        }
        const QByteArray iv = ss_random_bytes(kIvLen);
        if (iv.isEmpty())
            return false;
        QByteArray tag;
        const QByteArray ct = ss_gcm_encrypt(to_key, iv, plaintext, tag);
        if (tag.size() != kTagLen)
            return false;
        QSqlQuery up(conn);
        up.prepare(QStringLiteral("UPDATE secure_credentials SET ciphertext = ?, iv = ?, tag = ? WHERE key = ?"));
        up.addBindValue(ct);
        up.addBindValue(iv);
        up.addBindValue(tag);
        up.addBindValue(r.key);
        if (!up.exec()) {
            LOG_ERROR(kSecureSqliteTag, QString("Migration update failed key=%1 error=\"%2\"")
                                            .arg(r.key, up.lastError().text()));
            return false;
        }
        ++migrated_out;
    }
    return true;
}

struct DekLoad {
    QByteArray dek; // kKeyLen bytes on success
    SecretRead::Error error = SecretRead::Error::None;
    QString detail;
};

DekLoad ss_dek_fail(SecretRead::Error e, const QString& why) {
    DekLoad l;
    l.error = e;
    l.detail = why;
    return l;
}

// Load the DEK, creating (and migrating to) one only when the store
// demonstrably has none. See secure_detail::decide_dek for the rule.
DekLoad ss_load_or_create_dek() {
    auto& db = Database::instance();
    QSqlDatabase conn = db.connection();
    if (!conn.isOpen())
        return ss_dek_fail(SecretRead::Error::StoreClosed, QStringLiteral("no database connection on this thread"));

    secure_detail::DekInputs in;
#ifdef Q_OS_WIN
    in.dpapi_supported = true;
#endif
    WrappedRow dpapi_row;
    WrappedRow pbkdf2_row;
    QSqlError read_error;
    in.dpapi = ss_read_wrapped_row(conn, kDekDpapi, dpapi_row, &read_error);
    in.pbkdf2 = ss_read_wrapped_row(conn, kDekPbkdf2, pbkdf2_row, &read_error);

    QByteArray dpapi_dek;
    QByteArray pbkdf2_dek;
#ifdef Q_OS_WIN
    if (in.dpapi == secure_detail::RowState::Present)
        in.dpapi_unwrapped = ss_dpapi_unwrap(dpapi_row.ciphertext, dpapi_dek) && dpapi_dek.size() == kKeyLen;
#endif
    // The PBKDF2 copy is only needed when DPAPI did not deliver (200k iterations).
    if (in.pbkdf2 == secure_detail::RowState::Present && !in.dpapi_unwrapped)
        in.pbkdf2_unwrapped = ss_pbkdf2_unwrap(pbkdf2_row, pbkdf2_dek);

    const secure_detail::DekDecision d = secure_detail::decide_dek(in);
    switch (d.plan) {
        case secure_detail::DekPlan::UseDpapi: {
            // A PBKDF2 row alongside a working DPAPI one is a second, weaker copy
            // (an upgrade whose drop failed). It goes only when it wraps the SAME
            // key: one that wraps another key is the only way to rows encrypted
            // under that key. Rare (both rows present), so the PBKDF2 cost is fine.
            if (in.pbkdf2 == secure_detail::RowState::Present) {
                const bool old_unwrapped = ss_pbkdf2_unwrap(pbkdf2_row, pbkdf2_dek);
                if (secret_migration::may_drop_old_key_wrapping(true, dpapi_dek, old_unwrapped, pbkdf2_dek))
                    (void)ss_drop_row(conn, kDekPbkdf2);
                else
                    LOG_WARN(kSecureSqliteTag, "PBKDF2-wrapped key differs from the DPAPI one or cannot be unwrapped; "
                                               "both are kept");
            }
            DekLoad l;
            l.dek = dpapi_dek;
            return l;
        }
        case secure_detail::DekPlan::UsePbkdf2: {
#ifdef Q_OS_WIN
            ss_upgrade_wrapping_to_dpapi(conn, pbkdf2_dek);
#endif
            DekLoad l;
            l.dek = pbkdf2_dek;
            return l;
        }
        case secure_detail::DekPlan::Fail: {
            SecretRead::Error e = d.error;
            QString why = d.detail;
            if (e == SecretRead::Error::Io && read_error.type() != QSqlError::NoError) {
                e = ss_kind_of(read_error);
                why += QStringLiteral(": ") + read_error.text();
            }
            LOG_ERROR(kSecureSqliteTag, QString("Credential key could not be loaded kind=%1 detail=\"%2\" — no new key "
                                                "is created, saved credentials are left as they are")
                                            .arg(QLatin1String(SecretRead::error_name(e)), why));
            return ss_dek_fail(e, why);
        }
        case secure_detail::DekPlan::CreateNew:
            break;
    }

    // No wrapped key anywhere (both rows read fine, both absent): first use, or
    // an install that predates the DEK. Create one and migrate legacy rows.
    const QByteArray dek = ss_random_bytes(kKeyLen);
    if (dek.isEmpty())
        return ss_dek_fail(SecretRead::Error::Crypto, QStringLiteral("no random bytes for a new key"));

    if (auto t = db.begin_transaction(); t.is_err())
        return ss_dek_fail(SecretRead::Error::Busy, QStringLiteral("could not open a transaction for key creation: ") +
                                                        QString::fromStdString(t.error()));

    int migrated = 0;
    bool ok = ss_reencrypt_rows(conn, ss_legacy_key(), dek, migrated);
    const char* failed_step = ok ? "wrap" : "reencrypt"; // the step's own error is logged just before
    if (ok) {
        bool wrote = false;
#ifdef Q_OS_WIN
        QByteArray blob;
        if (ss_dpapi_wrap(dek, blob))
            wrote = ss_write_wrapped_row(conn, {QString::fromLatin1(kDekDpapi), blob, ss_unused_iv(), ss_unused_tag()});
        if (!wrote)
            LOG_WARN(kSecureSqliteTag, "DPAPI wrap unavailable — falling back to PBKDF2-wrapped key");
#endif
        if (!wrote) {
            WrappedRow row;
            wrote = ss_pbkdf2_wrap(dek, row) && ss_write_wrapped_row(conn, row);
        }
        ok = wrote;
    }
    if (!ok) {
        (void)db.rollback();
        LOG_ERROR(kSecureSqliteTag, logf("Credential key creation failed, store left unchanged",
                                         {{"step", failed_step}, {"rows_reencrypted", migrated}}));
        return ss_dek_fail(SecretRead::Error::Io, QStringLiteral("key creation failed"));
    }
    if (auto c = db.commit(); c.is_err()) {
        (void)db.rollback();
        return ss_dek_fail(SecretRead::Error::Busy,
                           QStringLiteral("key creation could not be committed: ") + QString::fromStdString(c.error()));
    }
    LOG_INFO(kSecureSqliteTag, QString("Credential key created legacy_rows_reencrypted=%1").arg(migrated));
    DekLoad l;
    l.dek = dek;
    return l;
}

/// The cached DEK. Success is cached for the session; a failure is retried by
/// the next caller after kKeyRetryAfterMs (never cached for the session).
struct KeyCache {
    QMutex mutex;
    QByteArray key;
    SecretRead::Error last_error = SecretRead::Error::None;
    QString last_detail;
    qint64 last_attempt_ms = 0;
};

KeyCache& ss_key_cache() {
    static KeyCache c;
    return c;
}

DekLoad ss_data_key() {
    KeyCache& c = ss_key_cache();
    QMutexLocker lock(&c.mutex);
    DekLoad out;
    if (!c.key.isEmpty()) {
        out.dek = c.key;
        return out;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (c.last_attempt_ms != 0 && now - c.last_attempt_ms < kKeyRetryAfterMs &&
        c.last_error != SecretRead::Error::None)
        return ss_dek_fail(c.last_error, c.last_detail);
    c.last_attempt_ms = now;
    DekLoad l = ss_load_or_create_dek();
    if (l.dek.size() == kKeyLen) {
        c.key = l.dek;
        c.last_error = SecretRead::Error::None;
        c.last_detail.clear();
    } else {
        c.last_error = l.error == SecretRead::Error::None ? SecretRead::Error::Io : l.error;
        c.last_detail = l.detail;
        l.error = c.last_error;
    }
    return l;
}

void ss_forget_key() {
    KeyCache& c = ss_key_cache();
    QMutexLocker lock(&c.mutex);
    c.key.fill('\0');
    c.key.clear();
    c.last_error = SecretRead::Error::None;
    c.last_detail.clear();
    c.last_attempt_ms = 0;
}

class SqliteSecretBackend final : public ISecretBackend {
  public:
    SecretRead read(const QString& key) override {
        auto& db = Database::instance();
        if (!db.is_open())
            return SecretRead::failure(SecretRead::Error::StoreClosed, QStringLiteral("database not open"));
        if (ss_is_reserved_key(key))
            return SecretRead::not_found();

        // Per-thread connection, NOT the shared handle: credential reads run on
        // worker threads (algo polling, order placement); one SQLite handle shared
        // across threads corrupts the parser.
        QSqlDatabase conn = db.connection();
        if (!conn.isOpen())
            return SecretRead::failure(SecretRead::Error::StoreClosed, QStringLiteral("no connection on this thread"));
        QSqlQuery q(conn);
        q.prepare(QStringLiteral("SELECT ciphertext, iv, tag FROM secure_credentials WHERE key = ?"));
        q.addBindValue(key);
        if (!q.exec())
            return SecretRead::failure(ss_kind_of(q.lastError()), q.lastError().text());
        if (!q.next())
            return SecretRead::not_found(); // definitive: the table was read and has no such row

        const QByteArray ciphertext = q.value(0).toByteArray();
        const QByteArray iv = q.value(1).toByteArray();
        const QByteArray tag = q.value(2).toByteArray();

        // A row exists: from here on nothing is "not found". The key is loaded only
        // now, so a store without the row never needs it.
        const DekLoad k = ss_data_key();
        if (k.dek.size() != kKeyLen)
            return SecretRead::failure(k.error, k.detail);
        if (iv.size() != kIvLen || tag.size() != kTagLen)
            return SecretRead::failure(SecretRead::Error::Corrupt,
                                       QStringLiteral("row has iv=%1 tag=%2 bytes").arg(iv.size()).arg(tag.size()));
        bool ok = false;
        const QByteArray plaintext = ss_gcm_decrypt(k.dek, iv, ciphertext, tag, ok);
        if (!ok) {
            // Damaged, tampered with, or encrypted under another key. NOT "not
            // found": a caller would then write a fresh value over the only copy.
            return SecretRead::failure(SecretRead::Error::Corrupt, QStringLiteral("authentication tag mismatch"));
        }
        return SecretRead::found_value(QString::fromUtf8(plaintext));
    }

    Result<void> write(const QString& key, const QString& value) override {
        auto& db = Database::instance();
        if (!db.is_open()) {
            LOG_ERROR(kSecureSqliteTag, "Secure storage write before the database was opened");
            return Result<void>::err("Database not open");
        }
        if (ss_is_reserved_key(key))
            return Result<void>::err("Reserved key");
        const DekLoad k = ss_data_key();
        if (k.dek.size() != kKeyLen)
            return Result<void>::err(QStringLiteral("Credential key unavailable (%1)")
                                         .arg(QLatin1String(SecretRead::error_name(k.error)))
                                         .toStdString());
        const QByteArray iv = ss_random_bytes(kIvLen);
        if (iv.isEmpty())
            return Result<void>::err("Encryption failed");
        QByteArray tag;
        const QByteArray ciphertext = ss_gcm_encrypt(k.dek, iv, value.toUtf8(), tag);
        if (tag.size() != kTagLen)
            return Result<void>::err("Encryption failed");

        QSqlDatabase conn = db.connection();
        if (!conn.isOpen())
            return Result<void>::err("DB connection unavailable on this thread");
        QSqlQuery q(conn);
        q.prepare(QStringLiteral(R"sql(
            INSERT INTO secure_credentials (key, ciphertext, iv, tag, updated_at)
            VALUES (?, ?, ?, ?, datetime('now'))
            ON CONFLICT(key) DO UPDATE SET
                ciphertext = excluded.ciphertext,
                iv         = excluded.iv,
                tag        = excluded.tag,
                updated_at = excluded.updated_at
        )sql"));
        q.addBindValue(key);
        q.addBindValue(ciphertext);
        q.addBindValue(iv);
        q.addBindValue(tag);
        if (!q.exec()) {
            LOG_ERROR(kSecureSqliteTag,
                      QString("Credential write failed key=%1 error=\"%2\"").arg(key, q.lastError().text()));
            return Result<void>::err("Failed to write credential row");
        }
        return Result<void>::ok();
    }

    Result<void> erase(const QString& key) override {
        auto& db = Database::instance();
        if (!db.is_open())
            return Result<void>::err("Database not open");
        if (ss_is_reserved_key(key))
            return Result<void>::err("Reserved key");
        QSqlDatabase conn = db.connection();
        if (!conn.isOpen())
            return Result<void>::err("DB connection unavailable on this thread");
        QSqlQuery q(conn);
        q.prepare(QStringLiteral("DELETE FROM secure_credentials WHERE key = ?"));
        q.addBindValue(key);
        if (!q.exec()) {
            LOG_ERROR(kSecureSqliteTag,
                      QString("Credential delete failed key=%1 error=\"%2\"").arg(key, q.lastError().text()));
            return Result<void>::err("Delete failed");
        }
        return Result<void>::ok();
    }

    Result<void> erase_all() override {
        auto& db = Database::instance();
        if (!db.is_open())
            return Result<void>::err("Database not open");
        QSqlDatabase conn = db.connection();
        if (!conn.isOpen())
            return Result<void>::err("DB connection unavailable on this thread");
        QSqlQuery q(conn);
        if (!q.exec(QStringLiteral("DELETE FROM secure_credentials")))
            return Result<void>::err(q.lastError().text().toStdString());
        ss_forget_key(); // the next write creates a fresh key
        return Result<void>::ok();
    }
};

} // namespace secure_sqlite_detail

std::unique_ptr<ISecretBackend> make_default_secret_backend() {
    return std::make_unique<secure_sqlite_detail::SqliteSecretBackend>();
}

} // namespace fincept
