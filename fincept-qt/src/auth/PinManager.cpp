#include "auth/PinManager.h"

#include "auth/SecurityAuditLog.h"
#include "core/logging/Logger.h"
#include "storage/secure/SecureStorage.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

#include <climits>

namespace fincept::auth {

namespace {

// Constant-time byte comparison. Returns true iff a and b are the same length
// AND every byte matches. The XOR-accumulate loop touches every byte even on
// mismatch so execution time does not reveal the first differing index.
bool constant_time_equals(const QByteArray& a, const QByteArray& b) {
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    const int n = a.size();
    for (int i = 0; i < n; ++i)
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return diff == 0;
}

// Reject trivially weak 6-digit PINs. Returns an error message or empty string
// if the PIN is acceptable. Centralized here (not in UI) so every caller —
// future QR/companion setup, tests, scripted flows — gets the same guardrails.
QString weak_pin_reason(const QString& pin) {
    if (pin.length() != 6)
        return QStringLiteral("PIN must be exactly 6 digits");
    for (const QChar& c : pin) {
        if (!c.isDigit())
            return QStringLiteral("PIN must contain only digits");
    }
    bool all_same = true;
    for (int i = 1; i < pin.length(); ++i) {
        if (pin[i] != pin[0]) { all_same = false; break; }
    }
    if (all_same)
        return QStringLiteral("PIN is too simple — use unique digits");

    bool seq_up = true, seq_down = true;
    for (int i = 1; i < pin.length(); ++i) {
        if (pin[i].unicode() != pin[i - 1].unicode() + 1) seq_up = false;
        if (pin[i].unicode() != pin[i - 1].unicode() - 1) seq_down = false;
    }
    if (seq_up || seq_down)
        return QStringLiteral("PIN is too simple — avoid sequential digits");

    return QString();
}

} // namespace

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

    // Check every write. A silent failure here previously meant an attacker
    // could hit the attempt counter with failures that never persisted,
    // bypassing lockout across restarts. If any write fails we log ERROR and
    // keep the in-memory counter authoritative for the rest of the session.
    const auto r_att = ss.store("pin_failed_attempts", QString::number(failed_attempts_));
    if (r_att.is_err())
        LOG_ERROR("Auth", QString("Failed to persist pin_failed_attempts: %1")
                              .arg(QString::fromStdString(r_att.error())));

    if (lockout_until_.isValid()) {
        const auto r_until = ss.store("pin_lockout_until", lockout_until_.toString(Qt::ISODate));
        if (r_until.is_err())
            LOG_ERROR("Auth", QString("Failed to persist pin_lockout_until: %1")
                                  .arg(QString::fromStdString(r_until.error())));

        // Record the wall clock observed at the moment we wrote the deadline.
        // On load, if current wall clock is earlier than this, the clock has
        // been rolled back and we refuse to clear the lockout.
        const auto r_stamp = ss.store("pin_lockout_stamp",
                                      QDateTime::currentDateTime().toString(Qt::ISODate));
        if (r_stamp.is_err())
            LOG_ERROR("Auth", QString("Failed to persist pin_lockout_stamp: %1")
                                  .arg(QString::fromStdString(r_stamp.error())));
    } else {
        const auto r1 = ss.remove("pin_lockout_until");
        const auto r2 = ss.remove("pin_lockout_stamp");
        if (r1.is_err())
            LOG_ERROR("Auth", QString("Failed to remove pin_lockout_until: %1")
                                  .arg(QString::fromStdString(r1.error())));
        if (r2.is_err())
            LOG_ERROR("Auth", QString("Failed to remove pin_lockout_stamp: %1")
                                  .arg(QString::fromStdString(r2.error())));
    }
}

void PinManager::load_lockout_state() {
    auto& ss = SecureStorage::instance();

    auto attempts_r = ss.retrieve("pin_failed_attempts");
    if (attempts_r.is_ok() && !attempts_r.value().isEmpty())
        failed_attempts_ = attempts_r.value().toInt();

    auto lockout_r = ss.retrieve("pin_lockout_until");
    if (lockout_r.is_ok() && !lockout_r.value().isEmpty()) {
        lockout_until_ = QDateTime::fromString(lockout_r.value(), Qt::ISODate);

        // Clock-rollback defence: compare current wall clock to the timestamp
        // we stored the last time we updated the lockout. If the clock has
        // been moved backward by more than 60s (small NTP corrections are
        // acceptable), refuse to trust "now" for lockout-expiry purposes —
        // extend the deadline by the rollback delta so the attacker gains no
        // free time.
        auto stamp_r = ss.retrieve("pin_lockout_stamp");
        const QDateTime now = QDateTime::currentDateTime();
        if (stamp_r.is_ok() && !stamp_r.value().isEmpty()) {
            const QDateTime stamp = QDateTime::fromString(stamp_r.value(), Qt::ISODate);
            if (stamp.isValid() && now.secsTo(stamp) > 60) {
                const qint64 rollback = now.secsTo(stamp); // positive seconds
                LOG_WARN("Auth",
                         QString("System clock rolled back %1s since last lockout write — "
                                 "extending lockout by that delta")
                             .arg(rollback));
                if (lockout_until_.isValid())
                    lockout_until_ = lockout_until_.addSecs(rollback);
            }
        }

        // Clear expired lockout only after the rollback check above.
        if (lockout_until_.isValid() && lockout_until_ <= now)
            lockout_until_ = QDateTime();
    }
}

// ── Public API ──────────────────────────────────────────────────────────────

bool PinManager::has_pin() const {
    return has_pin_;
}

Result<void> PinManager::set_pin(const QString& pin) {
    // Centralized weak-PIN rejection — every caller goes through this, so the
    // UI cannot be bypassed by a scripted flow or a future companion-setup path.
    const QString weak = weak_pin_reason(pin);
    if (!weak.isEmpty())
        return Result<void>::err(weak.toStdString());

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
    SecurityAuditLog::instance().record("pin_set");
    return Result<void>::ok();
}

Result<void> PinManager::change_pin(const QString& old_pin, const QString& new_pin) {
    // Must already have a PIN — otherwise the caller should go through set_pin.
    if (!has_pin_)
        return Result<void>::err("No PIN is configured — use set_pin()");

    // Verify old PIN via the same path as an unlock attempt. This means a wrong
    // old_pin increments failed_attempts_ and can trigger the normal lockout
    // ladder, so an attacker who momentarily has an unlocked terminal cannot
    // silently swap the PIN by guessing the old one. The audit log distinguishes
    // these failures from lock-screen failures via the source flag below — a
    // flood of source=change_pin failures has a different forensic meaning
    // (someone fiddling with Settings on an unattended unlocked terminal) than
    // source=lock_screen failures (someone trying to break in cold).
    audit_source_change_pin_ = true;
    const bool ok = verify_pin(old_pin);
    audit_source_change_pin_ = false;
    if (!ok)
        return Result<void>::err("Current PIN is incorrect");

    // Disallow reusing the same PIN so users don't think the flow did nothing.
    if (old_pin == new_pin)
        return Result<void>::err("New PIN must differ from the current PIN");

    auto r = set_pin(new_pin);
    if (r.is_ok())
        SecurityAuditLog::instance().record("pin_changed");
    return r;
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

    // Derive and compare. Use a constant-time comparison so execution time
    // does not leak the position of the first differing byte — PBKDF2 already
    // makes brute-force expensive, but the compare itself must not be a side
    // channel.
    QByteArray derived = derive_key(pin, stored_salt_);

    if (constant_time_equals(derived, stored_hash_)) {
        // Success — reset lockout
        failed_attempts_ = 0;
        lockout_until_ = QDateTime();
        save_lockout_state();
        LOG_INFO("Auth", "PIN verified successfully");
        SecurityAuditLog::instance().record("pin_verify_ok");
        return true;
    }

    // Failed attempt — tag the audit detail with the originating surface
    // (lock_screen vs change_pin) so the audit reader can distinguish a
    // brute-force attempt at the lock screen from a legitimate user
    // fat-fingering their old PIN inside Settings → Change PIN.
    failed_attempts_++;
    const QString source = audit_source_change_pin_ ? "change_pin" : "lock_screen";
    LOG_WARN("Auth", QString("PIN verification failed via %1 (attempt %2/%3)")
                          .arg(source).arg(failed_attempts_).arg(kMaxAttempts));
    SecurityAuditLog::instance().record(
        "pin_verify_fail",
        QString("attempt=%1/%2 source=%3").arg(failed_attempts_).arg(kMaxAttempts).arg(source));

    if (failed_attempts_ >= kMaxAttempts) {
        // Permanent lockout — require server re-auth
        save_lockout_state();
        SecurityAuditLog::instance().record("max_attempts_exceeded");
        emit max_attempts_exceeded();
        emit lockout_changed(true, 0);
        return false;
    }

    // Grace period: the first kFreeAttempts mistakes just show "incorrect"
    // with no timed lockout — users legitimately fat-finger a 6-digit PIN.
    // The timed-lockout ladder starts at attempt (kFreeAttempts + 1), i.e.
    // the 3rd failure triggers the first 30s lockout.
    if (failed_attempts_ <= kFreeAttempts) {
        save_lockout_state();
        return false;
    }

    // Apply timed lockout. The ladder is indexed from the first *post-grace*
    // failure so the first lockout is kLockoutTiers[0] (30s).
    int tier = qMin(failed_attempts_ - kFreeAttempts - 1,
                    static_cast<int>(kLockoutTiers.size()) - 1);
    int lockout_secs = kLockoutTiers[static_cast<size_t>(tier)];
    lockout_until_ = QDateTime::currentDateTime().addSecs(lockout_secs);
    save_lockout_state();
    SecurityAuditLog::instance().record(
        "lockout_started", QString("seconds=%1").arg(lockout_secs));

    emit lockout_changed(true, lockout_secs);
    return false;
}

void PinManager::clear_pin() {
    auto& ss = SecureStorage::instance();
    ss.remove("pin_hash");
    ss.remove("pin_salt");
    ss.remove("pin_failed_attempts");
    ss.remove("pin_lockout_until");
    ss.remove("pin_lockout_stamp");

    stored_hash_.clear();
    stored_salt_.clear();
    has_pin_ = false;
    failed_attempts_ = 0;
    lockout_until_ = QDateTime();

    LOG_INFO("Auth", "PIN cleared");
    SecurityAuditLog::instance().record("pin_cleared");
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
    SecurityAuditLog::instance().record("lockout_cleared");
}

} // namespace fincept::auth
