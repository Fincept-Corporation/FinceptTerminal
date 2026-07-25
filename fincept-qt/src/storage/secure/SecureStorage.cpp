#include "storage/secure/SecureStorage.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QByteArray>
#include <QCryptographicHash>
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

// ── SQLite-backed credential store ───────────────────────────────────────────
//
// Values live in `secure_credentials` (created by migration v029) encrypted
// with AES-256-GCM under a per-install random *data encryption key* (DEK).
// The DEK itself is stored in the same table under a reserved key, wrapped by
// a platform key-encryption key (KEK):
//
//   Windows  — DPAPI (CryptProtectData, user scope). The wrapping key lives in
//              the user's Windows profile and is itself protected by their
//              logon credentials. This is the meaningful control: the DB file
//              alone is useless, and *another local user cannot decrypt it*.
//   Other    — PBKDF2-HMAC-SHA256 over the machine-local seed with a random
//              per-install salt. Note honestly that the seed (machine ID) is
//              readable by any local process, so the iteration count is a
//              speed bump against offline DB theft, not a defence against a
//              local attacker. Wiring macOS Keychain / libsecret in here is
//              the real fix on those platforms.
//
// Why the indirection: it decouples "what encrypts the rows" from "what the
// platform can protect", so the KEK can be rotated (or upgraded to Keychain)
// by rewrapping 32 bytes instead of re-encrypting every credential.
//
// Migration is automatic and lossless. Installs predating the DEK derived the
// row key directly as SHA-256(machine seed); on first access we derive that
// legacy key, re-encrypt every row under a fresh DEK, and store the wrapped
// DEK — inside one transaction, so a crash mid-migration rolls back.
//
// Crypto is OpenSSL EVP — already linked for Google Sheets JWT signing, so no
// new dependency. All EVP allocations are stack-bounded and freed on every
// exit path; no heap thrash on the hot path.

static constexpr auto TAG = "SecureStorage";

namespace fincept {
namespace {

constexpr int kKeyLen = 32;  // AES-256
constexpr int kIvLen = 12;   // GCM-recommended nonce size (RFC 5116 §5)
constexpr int kTagLen = 16;  // GCM authentication tag (128 bits)
constexpr int kSaltLen = 16; // PBKDF2 salt
constexpr int kPbkdf2Iterations = 200000;

// Stable per-app salt mixed into the machine-ID hash so a different app
// on the same machine derives a different key. Bumping this rotates
// every key — only do that with a fresh DB.
constexpr const char* kAppSalt = "fincept-terminal/secure-storage/v1";

// Reserved rows holding the wrapped DEK. Never returned to callers.
constexpr const char* kDekPrefix = "__fincept_dek_v2";
constexpr const char* kDekDpapi = "__fincept_dek_v2_dpapi";
constexpr const char* kDekPbkdf2 = "__fincept_dek_v2_pbkdf2";

bool is_reserved_key(const QString& key) {
    return key.startsWith(QLatin1String(kDekPrefix));
}

QByteArray random_bytes(int n) {
    QByteArray out(n, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(out.data()), n) != 1) {
        LOG_ERROR(TAG, "RAND_bytes failed — refusing to produce weak key material");
        return {};
    }
    return out;
}

// The machine-local seed. Two stable inputs: the platform-provided machine ID
// (per-user on macOS, per-machine on Windows/Linux) and the OS product type.
// The app salt suffix prevents collisions with other apps hashing the same
// identifiers.
QByteArray machine_seed() {
    QByteArray seed;
    seed.append(QSysInfo::machineUniqueId());
    seed.append('\x1f');
    seed.append(QSysInfo::productType().toUtf8());
    seed.append('\x1f');
    seed.append(kAppSalt);
    return seed;
}

// Pre-DEK key derivation. Only used to read rows written by older installs.
QByteArray legacy_key() {
    return QCryptographicHash::hash(machine_seed(), QCryptographicHash::Sha256);
}

// AES-256-GCM encrypt. Returns ciphertext + the GCM tag through `tag_out`.
// On any OpenSSL error returns an empty QByteArray and logs — callers must
// check both isEmpty() AND tag_out.size() == kTagLen before persisting.
QByteArray aes_gcm_encrypt(const QByteArray& key, const QByteArray& iv, const QByteArray& plaintext,
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

        // Reserve plaintext.size() + block size; AES has 16-byte blocks but
        // GCM is a stream cipher (CTR-mode under the hood) so output length
        // equals input length. The +16 margin keeps us safe if EVP ever
        // reports cipher block size for compatibility reasons.
        out.resize(plaintext.size() + 16);
        int len = 0;
        if (1 != EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &len,
                                   reinterpret_cast<const unsigned char*>(plaintext.constData()), plaintext.size()))
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
        LOG_ERROR(TAG, QString("AES-GCM encrypt failed: OpenSSL err 0x%1").arg(ERR_get_error(), 0, 16));
        return {};
    }
    return out;
}

// AES-256-GCM decrypt with tag verification. Returns plaintext on success or
// an empty QByteArray if the tag doesn't verify (tampered ciphertext, wrong
// machine, corrupted DB row). Callers must NOT use the returned bytes if
// `ok_out` is false — an empty QByteArray is also a valid plaintext.
QByteArray aes_gcm_decrypt(const QByteArray& key, const QByteArray& iv, const QByteArray& ciphertext,
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
                                   reinterpret_cast<const unsigned char*>(ciphertext.constData()), ciphertext.size()))
            break;
        int written = len;

        // EVP_CTRL_GCM_SET_TAG must be called before EVP_DecryptFinal_ex —
        // Final is what actually verifies the tag and returns 0 on mismatch.
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen, const_cast<char*>(tag.constData())))
            break;

        if (1 != EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + written, &len))
            break; // tag mismatch — leave ok_out=false
        written += len;
        out.resize(written);
        ok_out = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    return out;
}

// ── DEK wrapping ────────────────────────────────────────────────────────────
// A wrapped DEK is stored in the (ciphertext, iv, tag) columns of its reserved
// row. Layout differs per scheme; the row key names the scheme so a reader
// always knows how to interpret the blobs.

#ifdef Q_OS_WIN
// DPAPI: ciphertext = the opaque DPAPI blob, iv/tag unused (empty).
bool dpapi_wrap(const QByteArray& dek, QByteArray& blob_out) {
    QByteArray entropy(kAppSalt);
    DATA_BLOB in{static_cast<DWORD>(dek.size()), reinterpret_cast<BYTE*>(const_cast<char*>(dek.constData()))};
    DATA_BLOB ent{static_cast<DWORD>(entropy.size()), reinterpret_cast<BYTE*>(const_cast<char*>(entropy.constData()))};
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"Fincept Terminal credential key", &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                          &out)) {
        LOG_WARN(TAG, QString("CryptProtectData failed (err %1)").arg(GetLastError()));
        return false;
    }
    blob_out = QByteArray(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return true;
}

bool dpapi_unwrap(const QByteArray& blob, QByteArray& dek_out) {
    QByteArray entropy(kAppSalt);
    DATA_BLOB in{static_cast<DWORD>(blob.size()), reinterpret_cast<BYTE*>(const_cast<char*>(blob.constData()))};
    DATA_BLOB ent{static_cast<DWORD>(entropy.size()), reinterpret_cast<BYTE*>(const_cast<char*>(entropy.constData()))};
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        LOG_WARN(TAG, QString("CryptUnprotectData failed (err %1)").arg(GetLastError()));
        return false;
    }
    dek_out = QByteArray(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return true;
}
#endif

// PBKDF2 KEK over the machine seed.
QByteArray pbkdf2_kek(const QByteArray& salt) {
    const QByteArray seed = machine_seed();
    QByteArray kek(kKeyLen, '\0');
    if (1 != PKCS5_PBKDF2_HMAC(seed.constData(), seed.size(), reinterpret_cast<const unsigned char*>(salt.constData()),
                               salt.size(), kPbkdf2Iterations, EVP_sha256(), kKeyLen,
                               reinterpret_cast<unsigned char*>(kek.data()))) {
        LOG_ERROR(TAG, "PBKDF2 derivation failed");
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
// no separate iv/tag. The columns are declared NOT NULL and a *default
// constructed* QByteArray binds as SQL NULL — which fails the insert and
// silently demotes us to the weaker wrapping scheme. Always bind real bytes.
QByteArray unused_iv() {
    return QByteArray(kIvLen, '\0');
}
QByteArray unused_tag() {
    return QByteArray(kTagLen, '\0');
}

// PBKDF2 scheme: ciphertext = salt(16) || wrapped_dek, iv = GCM nonce, tag = GCM tag.
bool pbkdf2_wrap(const QByteArray& dek, WrappedRow& out) {
    const QByteArray salt = random_bytes(kSaltLen);
    const QByteArray iv = random_bytes(kIvLen);
    if (salt.isEmpty() || iv.isEmpty())
        return false;
    const QByteArray kek = pbkdf2_kek(salt);
    if (kek.size() != kKeyLen)
        return false;

    QByteArray tag;
    const QByteArray wrapped = aes_gcm_encrypt(kek, iv, dek, tag);
    if (tag.size() != kTagLen)
        return false;

    out.key = QString::fromLatin1(kDekPbkdf2);
    out.ciphertext = salt + wrapped;
    out.iv = iv;
    out.tag = tag;
    return true;
}

bool pbkdf2_unwrap(const WrappedRow& row, QByteArray& dek_out) {
    if (row.ciphertext.size() <= kSaltLen)
        return false;
    const QByteArray salt = row.ciphertext.left(kSaltLen);
    const QByteArray wrapped = row.ciphertext.mid(kSaltLen);
    const QByteArray kek = pbkdf2_kek(salt);
    if (kek.size() != kKeyLen)
        return false;

    bool ok = false;
    dek_out = aes_gcm_decrypt(kek, row.iv, wrapped, row.tag, ok);
    return ok && dek_out.size() == kKeyLen;
}

// ── DEK load / create / migrate ─────────────────────────────────────────────

bool read_wrapped_row(QSqlDatabase conn, const char* key, WrappedRow& out) {
    QSqlQuery q(conn);
    q.prepare("SELECT ciphertext, iv, tag FROM secure_credentials WHERE key = ?");
    q.addBindValue(QString::fromLatin1(key));
    if (!q.exec() || !q.next())
        return false;
    out.key = QString::fromLatin1(key);
    out.ciphertext = q.value(0).toByteArray();
    out.iv = q.value(1).toByteArray();
    out.tag = q.value(2).toByteArray();
    return true;
}

bool write_wrapped_row(QSqlDatabase conn, const WrappedRow& row) {
    QSqlQuery q(conn);
    q.prepare(R"sql(
        INSERT INTO secure_credentials (key, ciphertext, iv, tag, updated_at)
        VALUES (?, ?, ?, ?, datetime('now'))
        ON CONFLICT(key) DO UPDATE SET
            ciphertext = excluded.ciphertext,
            iv         = excluded.iv,
            tag        = excluded.tag,
            updated_at = excluded.updated_at
    )sql");
    q.addBindValue(row.key);
    q.addBindValue(row.ciphertext);
    q.addBindValue(row.iv);
    q.addBindValue(row.tag);
    if (!q.exec()) {
        LOG_ERROR(TAG, QString("Failed to persist wrapped DEK: %1").arg(q.lastError().text()));
        return false;
    }
    return true;
}

bool drop_row(QSqlDatabase conn, const char* key) {
    QSqlQuery q(conn);
    q.prepare("DELETE FROM secure_credentials WHERE key = ?");
    q.addBindValue(QString::fromLatin1(key));
    return q.exec();
}

#ifdef Q_OS_WIN
// Re-wrap an existing DEK under DPAPI and retire the PBKDF2 copy. This is the
// payoff of wrapping a DEK rather than deriving the row key directly: upgrading
// the protection scheme rewrites 32 bytes and leaves every credential row
// untouched. Best-effort — on failure the PBKDF2 row stays authoritative and we
// retry next launch. Order matters: write the new row *before* dropping the old
// one, so an interrupt can never leave the DEK unrecoverable.
void upgrade_wrapping_to_dpapi(QSqlDatabase conn, const QByteArray& dek) {
    QByteArray blob;
    if (!dpapi_wrap(dek, blob))
        return;
    if (!write_wrapped_row(conn, {QString::fromLatin1(kDekDpapi), blob, unused_iv(), unused_tag()}))
        return;
    if (!drop_row(conn, kDekPbkdf2)) {
        LOG_WARN(TAG, "Wrote the DPAPI-wrapped key but could not drop the PBKDF2 row");
        return;
    }
    LOG_INFO(TAG, "Upgraded credential key wrapping from PBKDF2 to DPAPI");
}
#endif

// Re-encrypt every credential row from `from_key` to `to_key`. Rows that fail
// to decrypt are left untouched — they were already unreadable (profile copied
// from another machine), and destroying them would lose nothing but would make
// the failure harder to diagnose.
bool reencrypt_rows(QSqlDatabase conn, const QByteArray& from_key, const QByteArray& to_key, int& migrated_out) {
    QVector<WrappedRow> rows;
    {
        QSqlQuery sel(conn);
        if (!sel.exec("SELECT key, ciphertext, iv, tag FROM secure_credentials")) {
            LOG_ERROR(TAG, QString("Migration select failed: %1").arg(sel.lastError().text()));
            return false;
        }
        while (sel.next()) {
            const QString k = sel.value(0).toString();
            if (is_reserved_key(k))
                continue;
            rows.push_back({k, sel.value(1).toByteArray(), sel.value(2).toByteArray(), sel.value(3).toByteArray()});
        }
    }

    migrated_out = 0;
    for (const WrappedRow& r : rows) {
        bool ok = false;
        const QByteArray plaintext = aes_gcm_decrypt(from_key, r.iv, r.ciphertext, r.tag, ok);
        if (!ok)
            continue;

        const QByteArray iv = random_bytes(kIvLen);
        if (iv.isEmpty())
            return false;
        QByteArray tag;
        const QByteArray ct = aes_gcm_encrypt(to_key, iv, plaintext, tag);
        if (tag.size() != kTagLen)
            return false;

        QSqlQuery up(conn);
        up.prepare("UPDATE secure_credentials SET ciphertext = ?, iv = ?, tag = ? WHERE key = ?");
        up.addBindValue(ct);
        up.addBindValue(iv);
        up.addBindValue(tag);
        up.addBindValue(r.key);
        if (!up.exec()) {
            LOG_ERROR(TAG, QString("Migration update failed for %1: %2").arg(r.key, up.lastError().text()));
            return false;
        }
        ++migrated_out;
    }
    return true;
}

// Load the DEK, creating (and migrating to) one on first run. Returns an empty
// QByteArray if the store is unusable; callers must treat that as an error.
QByteArray load_or_create_dek() {
    auto& db = Database::instance();
    QSqlDatabase conn = db.connection();

    // 1. Try the platform-preferred wrapped DEK.
#ifdef Q_OS_WIN
    {
        WrappedRow row;
        if (read_wrapped_row(conn, kDekDpapi, row)) {
            QByteArray dek;
            if (dpapi_unwrap(row.ciphertext, dek) && dek.size() == kKeyLen) {
                // A PBKDF2 row alongside the DPAPI one is a second, weaker copy
                // of the same key — retire it.
                (void)drop_row(conn, kDekPbkdf2);
                return dek;
            }
            LOG_ERROR(TAG, "Stored DEK could not be unwrapped by DPAPI — stored credentials are "
                           "unreadable on this account and must be re-entered");
        }
    }
#endif
    {
        WrappedRow row;
        if (read_wrapped_row(conn, kDekPbkdf2, row)) {
            QByteArray dek;
            if (pbkdf2_unwrap(row, dek)) {
#ifdef Q_OS_WIN
                upgrade_wrapping_to_dpapi(conn, dek);
#endif
                return dek;
            }
            LOG_ERROR(TAG, "Stored DEK failed PBKDF2 unwrap — stored credentials are unreadable "
                           "on this machine and must be re-entered");
        }
    }

    // 2. No usable DEK — create one and migrate any legacy rows under it.
    const QByteArray dek = random_bytes(kKeyLen);
    if (dek.isEmpty())
        return {};

    if (db.begin_transaction().is_err()) {
        LOG_ERROR(TAG, "Could not open a transaction for DEK creation");
        return {};
    }

    int migrated = 0;
    bool ok = reencrypt_rows(conn, legacy_key(), dek, migrated);
    if (ok) {
        bool wrote = false;
#ifdef Q_OS_WIN
        QByteArray blob;
        if (dpapi_wrap(dek, blob))
            wrote = write_wrapped_row(conn, {QString::fromLatin1(kDekDpapi), blob, unused_iv(), unused_tag()});
        if (!wrote)
            LOG_WARN(TAG, "DPAPI wrap unavailable — falling back to PBKDF2-wrapped DEK");
#endif
        if (!wrote) {
            WrappedRow row;
            wrote = pbkdf2_wrap(dek, row) && write_wrapped_row(conn, row);
        }
        ok = wrote;
    }

    if (!ok) {
        (void)db.rollback();
        LOG_ERROR(TAG, "DEK creation failed — credential store left unchanged");
        return {};
    }
    if (db.commit().is_err()) {
        (void)db.rollback();
        LOG_ERROR(TAG, "DEK creation could not be committed");
        return {};
    }

    LOG_INFO(TAG, QString("Initialised wrapped credential key (%1 legacy row(s) re-encrypted)").arg(migrated));
    return dek;
}

// Derive once and cache. Called under QMutex so the first caller wins;
// subsequent calls return the cached buffer without touching the DB.
const QByteArray& data_key() {
    static QMutex m;
    static QByteArray cached;
    static bool attempted = false;
    QMutexLocker lock(&m);
    if (!cached.isEmpty() || attempted)
        return cached;
    attempted = true;
    cached = load_or_create_dek();
    return cached;
}

} // anonymous namespace

SecureStorage& SecureStorage::instance() {
    static SecureStorage s;
    return s;
}

Result<void> SecureStorage::store(const QString& key, const QString& value) {
    auto& db = Database::instance();
    if (!db.is_open()) {
        LOG_ERROR(TAG, "SecureStorage::store called before Database::open()");
        return Result<void>::err("Database not open");
    }
    if (is_reserved_key(key))
        return Result<void>::err("Reserved key");

    const QByteArray key_material = data_key();
    if (key_material.size() != kKeyLen)
        return Result<void>::err("Credential key unavailable");

    const QByteArray plaintext = value.toUtf8();
    const QByteArray iv = random_bytes(kIvLen);
    if (iv.isEmpty())
        return Result<void>::err("Encryption failed");
    QByteArray tag;
    const QByteArray ciphertext = aes_gcm_encrypt(key_material, iv, plaintext, tag);
    if (tag.size() != kTagLen) {
        // aes_gcm_encrypt already logged the OpenSSL error.
        return Result<void>::err("Encryption failed");
    }

    QSqlQuery q(db.connection()); // per-thread connection (see retrieve) — thread-safe
    q.prepare(R"sql(
        INSERT INTO secure_credentials (key, ciphertext, iv, tag, updated_at)
        VALUES (?, ?, ?, ?, datetime('now'))
        ON CONFLICT(key) DO UPDATE SET
            ciphertext = excluded.ciphertext,
            iv         = excluded.iv,
            tag        = excluded.tag,
            updated_at = excluded.updated_at
    )sql");
    q.addBindValue(key);
    q.addBindValue(ciphertext);
    q.addBindValue(iv);
    q.addBindValue(tag);
    if (!q.exec()) {
        LOG_ERROR(TAG, QString("SQL upsert failed for key %1: %2").arg(key, q.lastError().text()));
        return Result<void>::err("Failed to write credential row");
    }
    return Result<void>::ok();
}

Result<QString> SecureStorage::retrieve(const QString& key) {
    auto& db = Database::instance();
    if (!db.is_open())
        return Result<QString>::err("Database not open");
    if (is_reserved_key(key))
        return Result<QString>::err("Not found");

    const QByteArray key_material = data_key();
    if (key_material.size() != kKeyLen)
        return Result<QString>::err("Credential key unavailable");

    // Use the per-thread connection, NOT the shared raw_db(): credential reads run
    // on worker threads (algo quote polling, candle warm-up, order placement), and
    // sharing one SQLite handle across threads corrupts the parser → SIGSEGV.
    QSqlQuery q(db.connection());
    q.prepare("SELECT ciphertext, iv, tag FROM secure_credentials WHERE key = ?");
    q.addBindValue(key);
    if (!q.exec()) {
        LOG_ERROR(TAG, QString("SQL select failed for key %1: %2").arg(key, q.lastError().text()));
        return Result<QString>::err("Read failed");
    }
    if (!q.next())
        return Result<QString>::err("Not found");

    const QByteArray ciphertext = q.value(0).toByteArray();
    const QByteArray iv = q.value(1).toByteArray();
    const QByteArray tag = q.value(2).toByteArray();

    bool ok = false;
    const QByteArray plaintext = aes_gcm_decrypt(key_material, iv, ciphertext, tag, ok);
    if (!ok) {
        // Tag mismatch — either the row is corrupted, the ciphertext was
        // tampered with, or the profile was copied from another machine.
        // Surface as a plain "not found" so the caller's recovery path
        // (re-prompt for PIN, re-login, etc.) kicks in cleanly. Logged as
        // WARN so a forensic reader can still see it happened.
        LOG_WARN(TAG, QString("AES-GCM tag mismatch for key %1 — row unreadable").arg(key));
        return Result<QString>::err("Not found");
    }
    return Result<QString>::ok(QString::fromUtf8(plaintext));
}

Result<void> SecureStorage::remove(const QString& key) {
    auto& db = Database::instance();
    if (!db.is_open())
        return Result<void>::err("Database not open");
    if (is_reserved_key(key))
        return Result<void>::err("Reserved key");

    QSqlQuery q(db.connection()); // per-thread connection (see retrieve) — thread-safe
    q.prepare("DELETE FROM secure_credentials WHERE key = ?");
    q.addBindValue(key);
    if (!q.exec()) {
        LOG_ERROR(TAG, QString("SQL delete failed for key %1: %2").arg(key, q.lastError().text()));
        return Result<void>::err("Delete failed");
    }
    return Result<void>::ok();
}

} // namespace fincept
