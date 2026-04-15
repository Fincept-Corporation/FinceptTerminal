#include "auth/PinManager.h"

#include "core/logging/Logger.h"
#include "storage/secure/SecureStorage.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

#include <climits>

namespace fincept::auth {

PinManager& PinManager::instance() {
    static PinManager s;
    return s;
}

PinManager::PinManager() {
    load_state();
    load_lockout_state();
}

// ── HMAC-SHA256 (manual, no QMessageAuthenticationCode dependency) ──────────
// RFC 2104: HMAC(K, m) = H((K' ^ opad) || H((K' ^ ipad) || m))

static QByteArray hmac_sha256(const QByteArray& key, const QByteArray& message) {
    static constexpr int kBlockSize = 64; // SHA-256 block size

    // Step 1: Normalize key length
    QByteArray k = key;
    if (k.size() > kBlockSize)
        k = QCryptographicHash::hash(k, QCryptographicHash::Sha256);
    while (k.size() < kBlockSize)
        k.append('\0');

    // Step 2: Create inner/outer padded keys.
    // 0x36 = ipad byte (RFC 2104 §2): XOR'd with key to form inner-hash input.
    // 0x5C = opad byte (RFC 2104 §2): XOR'd with key to form outer-hash input.
    QByteArray ipad(kBlockSize, '\0');
    QByteArray opad(kBlockSize, '\0');
    for (int i = 0; i < kBlockSize; ++i) {
        ipad[i] = static_cast<char>(k[i] ^ 0x36);
        opad[i] = static_cast<char>(k[i] ^ 0x5C);
    }

    // Step 3: Inner hash — H(ipad || message)
    QCryptographicHash inner(QCryptographicHash::Sha256);
    inner.addData(ipad);
    inner.addData(message);
    QByteArray inner_hash = inner.result();

    // Step 4: Outer hash — H(opad || inner_hash)
    QCryptographicHash outer(QCryptographicHash::Sha256);
    outer.addData(opad);
    outer.addData(inner_hash);
    return outer.result();
}

// ── PBKDF2-SHA256 key derivation ────────────────────────────────────────────
// RFC 2898 Section 5.2 — PBKDF2 with PRF = HMAC-SHA256.

QByteArray PinManager::derive_key(const QString& pin, const QByteArray& salt) const {
    const QByteArray password = pin.toUtf8();

    // Only need 1 block for 32-byte output
    // U_1 = PRF(Password, Salt || INT_32_BE(1))
    QByteArray salt_block = salt;
    salt_block.append('\0'); // block index 1 = 0x00000001
    salt_block.append('\0');
    salt_block.append('\0');
    salt_block.append('\x01');

    QByteArray u = hmac_sha256(password, salt_block);
    QByteArray t = u;

    for (int iter = 1; iter < kIterations; ++iter) {
        u = hmac_sha256(password, u);
        for (int j = 0; j < t.size(); ++j)
            t[j] = static_cast<char>(t[j] ^ u[j]);
    }

    return t.left(kHashLength);
}

// ── State persistence ───────────────────────────────────────────────────────

void PinManager::load_state() {
    auto& ss = SecureStorage::instance();

    auto hash_r = ss.retrieve("pin_hash");
    auto salt_r = ss.retrieve("pin_salt");

    if (hash_r.is_ok() && salt_r.is_ok() && !hash_r.value().isEmpty() && !salt_r.value().isEmpty()) {
        stored_hash_ = QByteArray::fromHex(hash_r.value().toUtf8());
        stored_salt_ = QByteArray::fromHex(salt_r.value().toUtf8());
        has_pin_ = true;
    } else {
        has_pin_ = false;
        stored_hash_.clear();
        stored_salt_.clear();
    }
}

void PinManager::save_lockout_state() {
    auto& ss = SecureStorage::instance();
    ss.store("pin_failed_attempts", QString::number(failed_attempts_));
    if (lockout_until_.isValid())
        ss.store("pin_lockout_until", lockout_until_.toString(Qt::ISODate));
    else
        ss.remove("pin_lockout_until");
}

void PinManager::load_lockout_state() {
    auto& ss = SecureStorage::instance();

    auto attempts_r = ss.retrieve("pin_failed_attempts");
    if (attempts_r.is_ok() && !attempts_r.value().isEmpty())
        failed_attempts_ = attempts_r.value().toInt();

    auto lockout_r = ss.retrieve("pin_lockout_until");
    if (lockout_r.is_ok() && !lockout_r.value().isEmpty()) {
        lockout_until_ = QDateTime::fromString(lockout_r.value(), Qt::ISODate);
        // Clear expired lockout
        if (lockout_until_.isValid() && lockout_until_ <= QDateTime::currentDateTime())
            lockout_until_ = QDateTime();
    }
}

// ── Public API ──────────────────────────────────────────────────────────────

bool PinManager::has_pin() const {
    return has_pin_;
}

Result<void> PinManager::set_pin(const QString& pin) {
    if (pin.length() != 6)
        return Result<void>::err("PIN must be exactly 6 digits");

    // Validate all digits
    for (const QChar& c : pin) {
        if (!c.isDigit())
            return Result<void>::err("PIN must contain only digits");
    }

    // Generate cryptographically random salt
    QByteArray salt(kSaltLength, '\0');
    QRandomGenerator* rng = QRandomGenerator::system();
    for (int i = 0; i < kSaltLength; ++i)
        salt[i] = static_cast<char>(rng->bounded(256));

    // Derive key
    QByteArray hash = derive_key(pin, salt);

    // Store in SecureStorage
    auto& ss = SecureStorage::instance();
    auto r1 = ss.store("pin_hash", QString::fromUtf8(hash.toHex()));
    if (r1.is_err())
        return Result<void>::err("Failed to store PIN hash: " + r1.error());

    auto r2 = ss.store("pin_salt", QString::fromUtf8(salt.toHex()));
    if (r2.is_err())
        return Result<void>::err("Failed to store PIN salt: " + r2.error());

    // Update cached state
    stored_hash_ = hash;
    stored_salt_ = salt;
    has_pin_ = true;

    // Reset lockout on new PIN
    failed_attempts_ = 0;
    lockout_until_ = QDateTime();
    save_lockout_state();

    LOG_INFO("Auth", "PIN configured successfully");
    return Result<void>::ok();
}

bool PinManager::verify_pin(const QString& pin) {
    if (!has_pin_)
        return false;

    // Check lockout
    if (is_locked_out())
        return false;

    // Check max attempts — require full re-auth
    if (failed_attempts_ >= kMaxAttempts) {
        emit max_attempts_exceeded();
        return false;
    }

    // Derive and compare
    QByteArray derived = derive_key(pin, stored_salt_);

    if (derived == stored_hash_) {
        // Success — reset lockout
        failed_attempts_ = 0;
        lockout_until_ = QDateTime();
        save_lockout_state();
        LOG_INFO("Auth", "PIN verified successfully");
        return true;
    }

    // Failed attempt
    failed_attempts_++;
    LOG_WARN("Auth", QString("PIN verification failed (attempt %1/%2)").arg(failed_attempts_).arg(kMaxAttempts));

    if (failed_attempts_ >= kMaxAttempts) {
        // Permanent lockout — require server re-auth
        save_lockout_state();
        emit max_attempts_exceeded();
        emit lockout_changed(true, 0);
        return false;
    }

    // Apply timed lockout
    int tier = qMin(failed_attempts_ - 1, static_cast<int>(kLockoutTiers.size()) - 1);
    int lockout_secs = kLockoutTiers[static_cast<size_t>(tier)];
    lockout_until_ = QDateTime::currentDateTime().addSecs(lockout_secs);
    save_lockout_state();

    emit lockout_changed(true, lockout_secs);
    return false;
}

void PinManager::clear_pin() {
    auto& ss = SecureStorage::instance();
    ss.remove("pin_hash");
    ss.remove("pin_salt");
    ss.remove("pin_failed_attempts");
    ss.remove("pin_lockout_until");

    stored_hash_.clear();
    stored_salt_.clear();
    has_pin_ = false;
    failed_attempts_ = 0;
    lockout_until_ = QDateTime();

    LOG_INFO("Auth", "PIN cleared");
}

bool PinManager::is_locked_out() const {
    if (!lockout_until_.isValid())
        return false;
    return QDateTime::currentDateTime() < lockout_until_;
}

int PinManager::lockout_remaining_seconds() const {
    if (!lockout_until_.isValid())
        return 0;
    const qint64 secs = QDateTime::currentDateTime().secsTo(lockout_until_);
    return static_cast<int>(qBound(qint64{0}, secs, qint64{INT_MAX}));
}

void PinManager::reset_lockout() {
    failed_attempts_ = 0;
    lockout_until_ = QDateTime();
    save_lockout_state();
    emit lockout_changed(false, 0);
    LOG_INFO("Auth", "PIN lockout reset after re-authentication");
}

} // namespace fincept::auth
