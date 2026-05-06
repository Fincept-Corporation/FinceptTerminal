#include "storage/secure/SecureStorage.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QSysInfo>

#include <openssl/err.h>
#include <openssl/evp.h>

// ── SQLite-backed credential store ───────────────────────────────────────────
//
// Values live in `secure_credentials` (created by migration v025) encrypted
// with AES-256-GCM. The 256-bit key is derived once per process from a
// machine-local seed and cached in static storage; rotating the seed (e.g.
// after copying the profile to another machine) makes the existing rows
// undecryptable, which is the desired behaviour.
//
// Crypto is OpenSSL EVP — already linked transitively for Google Sheets JWT
// signing, so no new dependency. All EVP allocations are stack-bounded and
// freed on every exit path; no heap thrash on the hot path.

static constexpr auto TAG = "SecureStorage";

namespace fincept {
namespace {

constexpr int kKeyLen = 32;     // AES-256
constexpr int kIvLen = 12;      // GCM-recommended nonce size (RFC 5116 §5)
constexpr int kTagLen = 16;     // GCM authentication tag (128 bits)

// Stable per-app salt mixed into the machine-ID hash so a different app
// on the same machine derives a different key. Bumping this rotates
// every key — only do that with a fresh DB.
constexpr const char* kAppSalt = "fincept-terminal/secure-storage/v1";

// Derive the AES-256 key once and cache. Called via QMutex so the first
// caller wins; subsequent calls return the cached buffer without rehashing.
const QByteArray& master_key() {
    static QMutex m;
    static QByteArray cached;
    QMutexLocker lock(&m);
    if (!cached.isEmpty())
        return cached;

    // Two stable inputs: the platform-provided machine ID (per-user on
    // macOS, per-machine on Windows/Linux) and the OS product type. The
    // app salt suffix prevents collisions with other apps that hash the
    // same identifiers.
    QByteArray seed;
    seed.append(QSysInfo::machineUniqueId());
    seed.append('\x1f');
    seed.append(QSysInfo::productType().toUtf8());
    seed.append('\x1f');
    seed.append(kAppSalt);

    cached = QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
    Q_ASSERT(cached.size() == kKeyLen);
    return cached;
}

QByteArray random_iv() {
    QByteArray iv(kIvLen, '\0');
    QRandomGenerator* rng = QRandomGenerator::system();
    for (int i = 0; i < kIvLen; ++i)
        iv[i] = static_cast<char>(rng->bounded(256));
    return iv;
}

// AES-256-GCM encrypt. Returns ciphertext + the GCM tag through `tag_out`.
// On any OpenSSL error returns an empty QByteArray and logs — callers must
// check both isEmpty() AND tag_out.size() == kTagLen before persisting.
QByteArray aes_gcm_encrypt(const QByteArray& key, const QByteArray& iv,
                           const QByteArray& plaintext, QByteArray& tag_out) {
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
        if (1 != EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                                    reinterpret_cast<const unsigned char*>(key.constData()),
                                    reinterpret_cast<const unsigned char*>(iv.constData())))
            break;

        // Reserve plaintext.size() + block size; AES has 16-byte blocks but
        // GCM is a stream cipher (CTR-mode under the hood) so output length
        // equals input length. The +16 margin keeps us safe if EVP ever
        // reports cipher block size for compatibility reasons.
        out.resize(plaintext.size() + 16);
        int len = 0;
        if (1 != EVP_EncryptUpdate(ctx,
                                   reinterpret_cast<unsigned char*>(out.data()), &len,
                                   reinterpret_cast<const unsigned char*>(plaintext.constData()),
                                   plaintext.size()))
            break;
        int written = len;

        if (1 != EVP_EncryptFinal_ex(ctx,
                                     reinterpret_cast<unsigned char*>(out.data()) + written,
                                     &len))
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
        LOG_ERROR(TAG, QString("AES-GCM encrypt failed: OpenSSL err 0x%1")
                           .arg(ERR_get_error(), 0, 16));
        return {};
    }
    return out;
}

// AES-256-GCM decrypt with tag verification. Returns plaintext on success or
// an empty QByteArray if the tag doesn't verify (tampered ciphertext, wrong
// machine, corrupted DB row). Callers must NOT use the returned bytes if
// `ok_out` is false — an empty QByteArray is also a valid plaintext.
QByteArray aes_gcm_decrypt(const QByteArray& key, const QByteArray& iv,
                           const QByteArray& ciphertext, const QByteArray& tag,
                           bool& ok_out) {
    ok_out = false;
    if (iv.size() != kIvLen || tag.size() != kTagLen)
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
        if (1 != EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                                    reinterpret_cast<const unsigned char*>(key.constData()),
                                    reinterpret_cast<const unsigned char*>(iv.constData())))
            break;

        out.resize(ciphertext.size() + 16);
        int len = 0;
        if (1 != EVP_DecryptUpdate(ctx,
                                   reinterpret_cast<unsigned char*>(out.data()), &len,
                                   reinterpret_cast<const unsigned char*>(ciphertext.constData()),
                                   ciphertext.size()))
            break;
        int written = len;

        // EVP_CTRL_GCM_SET_TAG must be called before EVP_DecryptFinal_ex —
        // Final is what actually verifies the tag and returns 0 on mismatch.
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen,
                                     const_cast<char*>(tag.constData())))
            break;

        if (1 != EVP_DecryptFinal_ex(ctx,
                                     reinterpret_cast<unsigned char*>(out.data()) + written,
                                     &len))
            break;  // tag mismatch — leave ok_out=false
        written += len;
        out.resize(written);
        ok_out = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    return out;
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

    const QByteArray plaintext = value.toUtf8();
    const QByteArray iv = random_iv();
    QByteArray tag;
    const QByteArray ciphertext = aes_gcm_encrypt(master_key(), iv, plaintext, tag);
    if (tag.size() != kTagLen) {
        // aes_gcm_encrypt already logged the OpenSSL error.
        return Result<void>::err("Encryption failed");
    }

    QSqlQuery q(db.raw_db());
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
        LOG_ERROR(TAG, QString("SQL upsert failed for key %1: %2")
                           .arg(key, q.lastError().text()));
        return Result<void>::err("Failed to write credential row");
    }
    return Result<void>::ok();
}

Result<QString> SecureStorage::retrieve(const QString& key) {
    auto& db = Database::instance();
    if (!db.is_open())
        return Result<QString>::err("Database not open");

    QSqlQuery q(db.raw_db());
    q.prepare("SELECT ciphertext, iv, tag FROM secure_credentials WHERE key = ?");
    q.addBindValue(key);
    if (!q.exec()) {
        LOG_ERROR(TAG, QString("SQL select failed for key %1: %2")
                           .arg(key, q.lastError().text()));
        return Result<QString>::err("Read failed");
    }
    if (!q.next())
        return Result<QString>::err("Not found");

    const QByteArray ciphertext = q.value(0).toByteArray();
    const QByteArray iv = q.value(1).toByteArray();
    const QByteArray tag = q.value(2).toByteArray();

    bool ok = false;
    const QByteArray plaintext = aes_gcm_decrypt(master_key(), iv, ciphertext, tag, ok);
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

    QSqlQuery q(db.raw_db());
    q.prepare("DELETE FROM secure_credentials WHERE key = ?");
    q.addBindValue(key);
    if (!q.exec()) {
        LOG_ERROR(TAG, QString("SQL delete failed for key %1: %2")
                           .arg(key, q.lastError().text()));
        return Result<void>::err("Delete failed");
    }
    return Result<void>::ok();
}

} // namespace fincept
