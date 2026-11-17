#include "auth/PinManager.h"

#include "auth/ConstantTime.h"
#include "auth/SecurityAuditLog.h"
#include "core/logging/Logger.h"
#include "storage/secure/SecureStorage.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFuture>
#include <QFutureWatcher>
#include <QPasswordDigestor>
#include <QPointer>
#include <QRandomGenerator>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <openssl/evp.h>

#include <climits>
#include <iterator>
#include <utility>

namespace fincept::auth {

namespace {

/// The staged PIN record: "<salt hex>:<hash hex>" in ONE row (see store_pin).
constexpr const char* kPinStagedKey = "pin_record_next";

QString pin_staged_value(const QByteArray& hash, const QByteArray& salt) {
    return QString::fromLatin1(salt.toHex()) + QLatin1Char(':') + QString::fromLatin1(hash.toHex());
}

/// False when `v` is not a well-formed staged record of the expected sizes.
bool pin_parse_staged(const QString& v, int hash_len, int salt_len, QByteArray* hash, QByteArray* salt) {
    const qsizetype sep = v.indexOf(QLatin1Char(':'));
    if (sep <= 0)
        return false;
    const QByteArray s = QByteArray::fromHex(v.left(sep).toLatin1());
    const QByteArray h = QByteArray::fromHex(v.mid(sep + 1).toLatin1());
    if (s.size() != salt_len || h.size() != hash_len)
        return false;
    *hash = h;
    *salt = s;
    return true;
}

// constant_time_equals now lives in auth/ConstantTime.h so the loopback HTTP
// bridges share one implementation. Keeping a second copy in this anonymous
// namespace would make the unqualified call below ambiguous the moment a
// unity-build batch pulls both into the same translation unit.

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
        if (pin[i] != pin[0]) {
            all_same = false;
            break;
        }
    }
    if (all_same)
        return QStringLiteral("PIN is too simple — use unique digits");

    bool seq_up = true, seq_down = true;
    for (int i = 1; i < pin.length(); ++i) {
        if (pin[i].unicode() != pin[i - 1].unicode() + 1)
            seq_up = false;
        if (pin[i].unicode() != pin[i - 1].unicode() - 1)
            seq_down = false;
    }
    if (seq_up || seq_down)
        return QStringLiteral("PIN is too simple — avoid sequential digits");

    return QString();
}

} // namespace

// ── Asynchronous operations ─────────────────────────────────────────────────

struct PinManager::Op {
    enum class Kind { Verify, Set, Change };
    Kind kind = Kind::Verify;
    QString pin;     // Verify / Set: the PIN; Change: the current PIN
    QString new_pin; // Change only
    QPointer<QObject> context;
    bool has_context = false;
    VerifyCallback on_verify;
    ResultCallback on_result;

    // Filled in when the operation starts.
    quint64 generation = 0;
    QByteArray salt;       // Verify / Change: the stored salt; Set: the new salt
    QByteArray new_salt;   // Change: salt for the new PIN (empty: no new key needed)
    QString new_pin_error; // Change: why the new PIN is refused — reported only
                           // once the current PIN has been verified (old order)

    bool context_gone() const { return has_context && !context; }
};

struct PinManager::Derived {
    QByteArray key;     // derive_key(pin, salt)
    QByteArray new_key; // Change: derive_key(new_pin, new_salt)
    qint64 elapsed_ms = 0;
};

PinManager& PinManager::instance() {
    static PinManager s;
    return s;
}

PinManager::PinManager() {
    kdf_pool_ = new QThreadPool(this); // child: its destructor waits for a running derivation
    kdf_pool_->setObjectName(QStringLiteral("PinKdfPool"));
    kdf_pool_->setMaxThreadCount(2);
    load_state();
    if (pin_state_ == PinState::Unreadable && read_failure_.transient())
        schedule_auto_retry();
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

QByteArray PinManager::derive_key(const QString& pin, const QByteArray& salt) {
    const QByteArray password = pin.toUtf8();

    // OpenSSL's PBKDF2-HMAC-SHA256 (RFC 8018) — the same function with the same
    // parameters, so its output is byte-identical to Qt's and to the loop below
    // and every stored PIN hash still verifies. ~4x faster than
    // QPasswordDigestor (600k iterations: ~0.6 s against ~2.5 s on the same
    // loaded host), which is how long the lock screen shows "verifying".
    {
        QByteArray out(kHashLength, Qt::Uninitialized);
        if (PKCS5_PBKDF2_HMAC(password.constData(), static_cast<int>(password.size()),
                              reinterpret_cast<const unsigned char*>(salt.constData()), static_cast<int>(salt.size()),
                              kIterations, EVP_sha256(), kHashLength,
                              reinterpret_cast<unsigned char*>(out.data())) == 1)
            return out;
    }

    // Qt's own PBKDF2-HMAC-SHA256 yields byte-identical output (verified against
    // the hand-rolled loop below for the production parameters). Fallback should
    // OpenSSL report failure; the loop stays as the last resort should the
    // digestor ever fail too (it returns an empty array).
    {
        const QByteArray fast = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, password, salt,
                                                                   kIterations, static_cast<quint64>(kHashLength));
        if (fast.size() == kHashLength)
            return fast;
    }

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
    const PinState before = pin_state_;

    // Tri-state reads: a read ERROR is never "no PIN". Treating it as "no PIN"
    // sent a signed-in user to PIN setup — overwriting the real PIN — and let a
    // terminal start without its lock screen.
    auto unreadable = [this](const SecretRead& why) {
        pin_state_ = PinState::Unreadable;
        read_failure_ = why;
        has_pin_ = false;
        stored_hash_.clear();
        stored_salt_.clear();
    };

    // A staged record is the newest PIN: written and confirmed by store_pin(),
    // not yet (fully) copied into pin_hash / pin_salt.
    const SecretRead staged_r = ss.read(QString::fromLatin1(kPinStagedKey));
    QByteArray staged_hash;
    QByteArray staged_salt;
    const bool staged = staged_r.found() &&
                        pin_parse_staged(staged_r.value, kHashLength, kSaltLength, &staged_hash, &staged_salt);
    if (staged_r.found() && !staged)
        LOG_WARN_RL("Auth", QStringLiteral("pin_staged_malformed"), 3600 * 1000,
                    QStringLiteral("Staged PIN record is malformed; the saved PIN record is used"));

    const SecretRead hash_r = staged || staged_r.failed() ? SecretRead::not_found() : ss.read(QStringLiteral("pin_hash"));
    const SecretRead salt_r = staged || staged_r.failed() ? SecretRead::not_found() : ss.read(QStringLiteral("pin_salt"));

    if (staged_r.failed()) {
        unreadable(staged_r);
    } else if (staged) {
        stored_hash_ = staged_hash;
        stored_salt_ = staged_salt;
        has_pin_ = true;
        pin_state_ = PinState::Set;
        read_failure_ = SecretRead::not_found();
        if (!staged_commit_attempted_) {
            staged_commit_attempted_ = true; // once per session; the next start retries
            commit_staged_pin(staged_hash, staged_salt);
        }
    } else if (hash_r.failed()) {
        unreadable(hash_r);
    } else if (salt_r.failed()) {
        unreadable(salt_r);
    } else if (hash_r.is_not_found() && salt_r.is_not_found()) {
        pin_state_ = PinState::NotSet;
        read_failure_ = SecretRead::not_found();
        has_pin_ = false;
        stored_hash_.clear();
        stored_salt_.clear();
    } else {
        // Both rows read. Half a record (a crash between the two writes) or a
        // value that is not the hex of the right length is a damaged record —
        // still never "no PIN".
        const QByteArray hash = hash_r.found() ? QByteArray::fromHex(hash_r.value.toUtf8()) : QByteArray();
        const QByteArray salt = salt_r.found() ? QByteArray::fromHex(salt_r.value.toUtf8()) : QByteArray();
        if (hash.size() != kHashLength || salt.size() != kSaltLength) {
            unreadable(SecretRead::failure(SecretRead::Error::Corrupt,
                                           QStringLiteral("PIN record incomplete or malformed (hash=%1 salt=%2 bytes)")
                                               .arg(hash.size())
                                               .arg(salt.size())));
        } else {
            stored_hash_ = hash;
            stored_salt_ = salt;
            has_pin_ = true;
            pin_state_ = PinState::Set;
            read_failure_ = SecretRead::not_found();
        }
    }

    // The lockout ladder is security state too: an unreadable attempt counter
    // must not reset it to zero.
    if (pin_state_ != PinState::Unreadable) {
        SecretRead lockout_failure;
        if (!load_lockout_state(&lockout_failure))
            unreadable(lockout_failure);
    }

    if (pin_state_ == PinState::Unreadable) {
        LOG_WARN("Auth", QStringLiteral("PIN record could not be read kind=%1 transient=%2 — terminal stays locked")
                             .arg(QLatin1String(read_failure_.error_name()),
                                  read_failure_.transient() ? QStringLiteral("yes") : QStringLiteral("no")));
    } else if (before == PinState::Unreadable) {
        LOG_INFO("Auth", QStringLiteral("PIN record readable again state=%1")
                             .arg(pin_state_ == PinState::Set ? QStringLiteral("set") : QStringLiteral("not_set")));
    }
}

void PinManager::reload() {
    const PinState before = pin_state_;
    const SecretRead::Error before_error = read_failure_.error;
    retry_scheduled_ = false;
    load_state();
    if (pin_state_ == PinState::Unreadable) {
        if (read_failure_.transient())
            schedule_auto_retry();
    } else {
        auto_retry_step_ = 0;
        ++pin_generation_; // a derivation started against the unreadable state is stale
    }
    if (pin_state_ != before || read_failure_.error != before_error)
        emit pin_state_changed();
}

void PinManager::schedule_auto_retry() {
    // Back-off for a transient failure (database busy / not open yet): 0.5 s,
    // 2 s, 5 s, then every 15 s while the blocked screen is up. The user's Retry
    // calls reload() at once.
    static constexpr int kDelaysMs[] = {500, 2000, 5000, 15000};
    if (retry_scheduled_)
        return;
    retry_scheduled_ = true;
    const int idx = qMin(auto_retry_step_, static_cast<int>(std::size(kDelaysMs)) - 1);
    ++auto_retry_step_;
    QTimer::singleShot(kDelaysMs[idx], this, [this]() {
        if (!retry_scheduled_)
            return; // reload() ran meanwhile
        reload();
    });
}

void PinManager::save_lockout_state() {
    auto& ss = SecureStorage::instance();

    // Check every write. A silent failure here previously meant an attacker
    // could hit the attempt counter with failures that never persisted,
    // bypassing lockout across restarts. If any write fails we log ERROR and
    // keep the in-memory counter authoritative for the rest of the session.
    const auto r_att = ss.store("pin_failed_attempts", QString::number(failed_attempts_));
    if (r_att.is_err())
        LOG_ERROR("Auth",
                  QString("Failed to persist pin_failed_attempts: %1").arg(QString::fromStdString(r_att.error())));

    if (lockout_until_.isValid()) {
        const auto r_until = ss.store("pin_lockout_until", lockout_until_.toString(Qt::ISODate));
        if (r_until.is_err())
            LOG_ERROR("Auth",
                      QString("Failed to persist pin_lockout_until: %1").arg(QString::fromStdString(r_until.error())));

        // Record the wall clock observed at the moment we wrote the deadline.
        // On load, if current wall clock is earlier than this, the clock has
        // been rolled back and we refuse to clear the lockout.
        const auto r_stamp = ss.store("pin_lockout_stamp", QDateTime::currentDateTime().toString(Qt::ISODate));
        if (r_stamp.is_err())
            LOG_ERROR("Auth",
                      QString("Failed to persist pin_lockout_stamp: %1").arg(QString::fromStdString(r_stamp.error())));
    } else {
        const auto r1 = ss.remove("pin_lockout_until");
        const auto r2 = ss.remove("pin_lockout_stamp");
        if (r1.is_err())
            LOG_ERROR("Auth",
                      QString("Failed to remove pin_lockout_until: %1").arg(QString::fromStdString(r1.error())));
        if (r2.is_err())
            LOG_ERROR("Auth",
                      QString("Failed to remove pin_lockout_stamp: %1").arg(QString::fromStdString(r2.error())));
    }
}

bool PinManager::load_lockout_state(SecretRead* failure) {
    auto& ss = SecureStorage::instance();

    const SecretRead attempts_r = ss.read(QStringLiteral("pin_failed_attempts"));
    if (attempts_r.failed()) {
        if (failure)
            *failure = attempts_r;
        return false;
    }
    // The higher of memory and disk: when a counter write failed earlier this
    // session the in-memory count is the authoritative one (save_lockout_state).
    if (attempts_r.found() && !attempts_r.value.isEmpty())
        failed_attempts_ = qMax(failed_attempts_, attempts_r.value.toInt());

    const SecretRead lockout_r = ss.read(QStringLiteral("pin_lockout_until"));
    if (lockout_r.failed()) {
        if (failure)
            *failure = lockout_r;
        return false;
    }
    if (lockout_r.found() && !lockout_r.value.isEmpty()) {
        const QDateTime stored_until = QDateTime::fromString(lockout_r.value, Qt::ISODate);
        if (!lockout_until_.isValid() || (stored_until.isValid() && stored_until > lockout_until_))
            lockout_until_ = stored_until;

        // Clock-rollback defence: compare current wall clock to the timestamp
        // we stored the last time we updated the lockout. If the clock has
        // been moved backward by more than 60s (small NTP corrections are
        // acceptable), refuse to trust "now" for lockout-expiry purposes —
        // extend the deadline by the rollback delta so the attacker gains no
        // free time.
        const SecretRead stamp_r = ss.read(QStringLiteral("pin_lockout_stamp"));
        if (stamp_r.failed()) {
            if (failure)
                *failure = stamp_r;
            return false;
        }
        const QDateTime now = QDateTime::currentDateTime();
        if (stamp_r.found() && !stamp_r.value.isEmpty()) {
            const QDateTime stamp = QDateTime::fromString(stamp_r.value, Qt::ISODate);
            if (stamp.isValid() && now.secsTo(stamp) > 60) {
                const qint64 rollback = now.secsTo(stamp); // positive seconds
                LOG_WARN("Auth", QString("System clock rolled back %1s since last lockout write — "
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
    return true;
}

// ── Public API ──────────────────────────────────────────────────────────────

bool PinManager::has_pin() const {
    // Fail closed: an unreadable record locks like a configured PIN.
    return pin_state_ != PinState::NotSet;
}

void PinManager::set_pin_async(const QString& pin, QObject* context, ResultCallback done) {
    auto op = std::make_shared<Op>();
    op->kind = Op::Kind::Set;
    op->pin = pin;
    op->context = context;
    op->has_context = context != nullptr;
    op->on_result = std::move(done);
    queue_.push_back(std::move(op));
    schedule_pump();
}

void PinManager::change_pin_async(const QString& old_pin, const QString& new_pin, QObject* context,
                                  ResultCallback done) {
    auto op = std::make_shared<Op>();
    op->kind = Op::Kind::Change;
    op->pin = old_pin;
    op->new_pin = new_pin;
    op->context = context;
    op->has_context = context != nullptr;
    op->on_result = std::move(done);
    queue_.push_back(std::move(op));
    schedule_pump();
}

void PinManager::verify_pin_async(const QString& pin, QObject* context, VerifyCallback done) {
    auto op = std::make_shared<Op>();
    op->kind = Op::Kind::Verify;
    op->pin = pin;
    op->context = context;
    op->has_context = context != nullptr;
    op->on_verify = std::move(done);
    queue_.push_back(std::move(op));
    schedule_pump();
}

void PinManager::cancel_pending(QObject* context) {
    if (!context)
        return;
    for (auto it = queue_.begin(); it != queue_.end();) {
        if ((*it)->has_context && (*it)->context == context)
            it = queue_.erase(it);
        else
            ++it;
    }
}

bool PinManager::is_busy() const {
    return running_ != nullptr || !queue_.empty();
}

void PinManager::schedule_pump() {
    // Callbacks never run inside the call that queued the operation.
    if (pump_scheduled_ || running_)
        return;
    pump_scheduled_ = true;
    QTimer::singleShot(0, this, &PinManager::pump);
}

bool PinManager::verify_allowed() {
    if (pin_state_ == PinState::Unreadable)
        reload(); // a retry costs two small reads; succeed if the store is back
    if (pin_state_ != PinState::Set || !has_pin_)
        return false; // nothing to compare against — and never counted as a failed attempt

    // Check lockout
    if (is_locked_out())
        return false;

    // Check max attempts — require full re-auth
    if (failed_attempts_ >= kMaxAttempts) {
        emit max_attempts_exceeded();
        return false;
    }
    return true;
}

QByteArray PinManager::random_salt() const {
    // Generate cryptographically random salt
    QByteArray salt(kSaltLength, '\0');
    QRandomGenerator* rng = QRandomGenerator::system();
    for (int i = 0; i < kSaltLength; ++i)
        salt[i] = static_cast<char>(rng->bounded(256));
    return salt;
}

void PinManager::pump() {
    pump_scheduled_ = false;
    while (!running_ && !queue_.empty()) {
        std::shared_ptr<Op> op = std::move(queue_.front());
        queue_.pop_front();
        // The caller went away before its turn: nothing was attempted, so
        // there is nothing to count.
        if (op->context_gone())
            continue;

        switch (op->kind) {
            case Op::Kind::Verify:
                if (!verify_allowed()) {
                    finish(op, false, Result<void>::ok());
                    continue;
                }
                op->salt = stored_salt_;
                break;

            case Op::Kind::Set: {
                // Never write a PIN over a record that could not be read: it may hold
                // the user's real PIN (and "no PIN" would be a lock-screen bypass).
                if (pin_state_ == PinState::Unreadable)
                    reload();
                if (pin_state_ == PinState::Unreadable) {
                    finish(op, false,
                           Result<void>::err(QStringLiteral("Could not read secure storage: %1. Your existing PIN was "
                                                            "not changed.")
                                                 .arg(read_failure_.reason())
                                                 .toStdString()));
                    continue;
                }
                // Centralized weak-PIN rejection — every caller goes through this, so the
                // UI cannot be bypassed by a scripted flow or a future companion-setup path.
                const QString weak = weak_pin_reason(op->pin);
                if (!weak.isEmpty()) {
                    finish(op, false, Result<void>::err(weak.toStdString()));
                    continue;
                }
                op->salt = random_salt();
                break;
            }

            case Op::Kind::Change:
                // Must already have a (readable) PIN — otherwise the caller should go through set_pin.
                if (pin_state_ != PinState::Set) {
                    finish(op, false, Result<void>::err("No PIN is configured — use set_pin()"));
                    continue;
                }
                // Verify the old PIN through the same gate as an unlock attempt. This
                // means a wrong old_pin increments failed_attempts_ and can trigger the
                // normal lockout ladder, so an attacker who momentarily has an unlocked
                // terminal cannot silently swap the PIN by guessing the old one.
                if (!verify_allowed()) {
                    finish(op, false, Result<void>::err("Current PIN is incorrect"));
                    continue;
                }
                op->salt = stored_salt_;
                // The new PIN's own checks are reported only after the current PIN
                // has been verified (reuse first, then weakness — the old order), but
                // its key is derived alongside the current one so the user waits for
                // one derivation, not two in a row.
                if (op->pin == op->new_pin)
                    op->new_pin_error = QStringLiteral("New PIN must differ from the current PIN");
                else
                    op->new_pin_error = weak_pin_reason(op->new_pin);
                if (op->new_pin_error.isEmpty())
                    op->new_salt = random_salt();
                break;
        }

        op->generation = pin_generation_;
        running_ = op;
        start_derivation(op);
    }
}

void PinManager::start_derivation(const std::shared_ptr<Op>& op) {
    const QString pin = op->pin;
    const QByteArray salt = op->salt;
    const QString new_pin = op->new_salt.isEmpty() ? QString() : op->new_pin;
    const QByteArray new_salt = op->new_salt;
    QThreadPool* const pool = kdf_pool_;

    // The watcher is a child of this object, so its finished() — the only way
    // the result comes back — can never reach a destroyed PinManager. Connected
    // before setFuture() so an instantly finished future is not missed.
    auto* watcher = new QFutureWatcher<Derived>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, op]() {
        const Derived d = watcher->result();
        watcher->deleteLater();
        on_derived(op, d);
    });
    watcher->setFuture(QtConcurrent::run(pool, [pin, salt, new_pin, new_salt, pool]() {
        QElapsedTimer clock;
        clock.start();
        Derived d;
        // change_pin: the new PIN's key is derived on the pool's second thread
        // while this one checks the current PIN. result() runs the task inline
        // if it has not started yet, so this cannot deadlock.
        QFuture<QByteArray> next;
        if (!new_salt.isEmpty())
            next = QtConcurrent::run(pool, [new_pin, new_salt]() { return derive_key(new_pin, new_salt); });
        d.key = derive_key(pin, salt);
        if (!new_salt.isEmpty())
            d.new_key = next.result();
        d.elapsed_ms = clock.elapsed();
        return d;
    }));
}

void PinManager::on_derived(const std::shared_ptr<Op>& op, const Derived& d) {
    running_.reset();
    LOG_DEBUG("Auth", QString("PIN key derivation finished duration_ms=%1").arg(d.elapsed_ms));

    // Set / clear while the key was being derived: the attempt was made against
    // a PIN that no longer exists. Never count it and never let it unlock.
    const bool pin_unchanged = op->generation == pin_generation_;
    bool ok = false;
    Result<void> result = Result<void>::ok();

    switch (op->kind) {
        case Op::Kind::Verify:
            if (!pin_unchanged) {
                LOG_WARN("Auth", "PIN was changed or cleared while an unlock attempt was being checked — "
                                 "attempt discarded");
                break;
            }
            // Constant-time comparison so execution time does not leak the position
            // of the first differing byte — PBKDF2 already makes brute force
            // expensive, but the compare itself must not be a side channel.
            ok = account_verify(constant_time_equals(d.key, stored_hash_), QStringLiteral("lock_screen"));
            break;

        case Op::Kind::Set:
            if (!pin_unchanged) {
                // Only clear_pin() can intervene (sign-out / re-auth): it wins.
                LOG_WARN("Auth", "PIN was cleared while a new PIN was being saved — not stored");
                result = Result<void>::err("PIN setup was interrupted — please try again");
                break;
            }
            result = store_pin(d.key, op->salt);
            ok = result.is_ok();
            break;

        case Op::Kind::Change:
            if (!pin_unchanged) {
                LOG_WARN("Auth", "PIN was cleared while a PIN change was being checked — change discarded");
                result = Result<void>::err("No PIN is configured — use set_pin()");
                break;
            }
            // The audit log distinguishes these failures from lock-screen failures
            // via the source tag — a flood of source=change_pin failures has a
            // different forensic meaning (someone fiddling with Settings on an
            // unattended unlocked terminal) than source=lock_screen failures.
            if (!account_verify(constant_time_equals(d.key, stored_hash_), QStringLiteral("change_pin"))) {
                result = Result<void>::err("Current PIN is incorrect");
                break;
            }
            // Disallow reusing the same PIN so users don't think the flow did nothing.
            if (!op->new_pin_error.isEmpty()) {
                result = Result<void>::err(op->new_pin_error.toStdString());
                break;
            }
            result = store_pin(d.new_key, op->new_salt);
            if (result.is_ok()) {
                SecurityAuditLog::instance().record("pin_changed");
                LOG_AUDIT("Auth", "PIN changed");
            }
            ok = result.is_ok();
            break;
    }

    finish(op, ok, result);
    pump();
}

void PinManager::finish(const std::shared_ptr<Op>& op, bool ok, const Result<void>& result) {
    if (op->context_gone())
        return;
    if (op->kind == Op::Kind::Verify) {
        if (op->on_verify)
            op->on_verify(ok);
    } else if (op->on_result) {
        op->on_result(result);
    }
}

bool PinManager::commit_staged_pin(const QByteArray& hash, const QByteArray& salt) {
    auto& ss = SecureStorage::instance();
    const auto s = ss.store_verified(QStringLiteral("pin_salt"), QString::fromLatin1(salt.toHex()));
    const auto h = s.is_ok() ? ss.store_verified(QStringLiteral("pin_hash"), QString::fromLatin1(hash.toHex())) : s;
    const auto rm = h.is_ok() ? ss.remove(QString::fromLatin1(kPinStagedKey)) : h;
    const bool failing = rm.is_err();
    // The staged row stays the PIN until this works; one line per session.
    LOG_TRANSITION("Auth", QStringLiteral("pin_record_commit"), failing, failing ? LogLevel::Warn : LogLevel::Info,
                   failing ? logf("PIN record not fully written; the new PIN stays in force from the staged record "
                                  "and the write is retried at the next start",
                                  {{"error", QString::fromStdString(rm.error())}})
                           : QStringLiteral("PIN record written"));
    return !failing;
}

Result<void> PinManager::store_pin(const QByteArray& hash, const QByteArray& salt) {
    // A PIN record is two rows. Written one after the other, a failure between
    // them left the new hash beside the old salt: neither PIN unlocked any more.
    // The new record is first written as ONE row, read back and compared — from
    // then on it IS the PIN (load_state() prefers it) — and only then copied into
    // the two rows; a failed copy keeps the staged row and is retried.
    auto& ss = SecureStorage::instance();
    const auto staged = ss.store_verified(QString::fromLatin1(kPinStagedKey), pin_staged_value(hash, salt));
    if (staged.is_err()) {
        // Not written, or not confirmed: the current PIN stays in force. A row that
        // was written but could not be read back must not take effect later.
        const auto rm = ss.remove(QString::fromLatin1(kPinStagedKey));
        if (rm.is_err())
            LOG_ERROR("Auth", logf("Unconfirmed staged PIN record could not be removed",
                                   {{"error", QString::fromStdString(rm.error())}}));
        return Result<void>::err("Failed to store PIN: " + staged.error());
    }
    commit_staged_pin(hash, salt);

    // Update cached state
    stored_hash_ = hash;
    stored_salt_ = salt;
    has_pin_ = true;
    const bool state_changed = pin_state_ != PinState::Set;
    pin_state_ = PinState::Set;
    read_failure_ = SecretRead::not_found();
    ++pin_generation_;
    if (state_changed)
        emit pin_state_changed();

    // Reset lockout on new PIN
    failed_attempts_ = 0;
    lockout_until_ = QDateTime();
    save_lockout_state();

    LOG_AUDIT("Auth", "PIN set");
    SecurityAuditLog::instance().record("pin_set");
    return Result<void>::ok();
}

bool PinManager::account_verify(bool match, const QString& source) {
    if (match) {
        // Success — reset lockout
        failed_attempts_ = 0;
        lockout_until_ = QDateTime();
        save_lockout_state();
        // The event line is "Terminal unlocked" (InactivityGuard::set_terminal_locked).
        LOG_DEBUG("Auth", "PIN verified successfully");
        SecurityAuditLog::instance().record("pin_verify_ok");
        return true;
    }

    // Failed attempt — tag the audit detail with the originating surface
    // (lock_screen vs change_pin) so the audit reader can distinguish a
    // brute-force attempt at the lock screen from a legitimate user
    // fat-fingering their old PIN inside Settings → Change PIN.
    failed_attempts_++;
    LOG_WARN(
        "Auth",
        QString("PIN verification failed via %1 (attempt %2/%3)").arg(source).arg(failed_attempts_).arg(kMaxAttempts));
    SecurityAuditLog::instance().record(
        "pin_verify_fail", QString("attempt=%1/%2 source=%3").arg(failed_attempts_).arg(kMaxAttempts).arg(source));

    if (failed_attempts_ >= kMaxAttempts) {
        // Permanent lockout — require server re-auth
        save_lockout_state();
        SecurityAuditLog::instance().record("max_attempts_exceeded");
        LOG_AUDIT("Auth", "PIN locked after too many wrong attempts", {{"next", "server sign-in required"}});
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
    int tier = qMin(failed_attempts_ - kFreeAttempts - 1, static_cast<int>(kLockoutTiers.size()) - 1);
    int lockout_secs = kLockoutTiers[static_cast<size_t>(tier)];
    lockout_until_ = QDateTime::currentDateTime().addSecs(lockout_secs);
    save_lockout_state();
    LOG_WARN("Auth", logf("PIN lockout started", {{"seconds", lockout_secs}, {"failed_attempts", failed_attempts_}}));
    SecurityAuditLog::instance().record("lockout_started", QString("seconds=%1").arg(lockout_secs));

    emit lockout_changed(true, lockout_secs);
    return false;
}

void PinManager::clear_pin() {
    auto& ss = SecureStorage::instance();
    ss.remove(QString::fromLatin1(kPinStagedKey)); // first: a staged record is the PIN in force
    ss.remove("pin_hash");
    ss.remove("pin_salt");
    ss.remove("pin_failed_attempts");
    ss.remove("pin_lockout_until");
    ss.remove("pin_lockout_stamp");

    stored_hash_.clear();
    stored_salt_.clear();
    has_pin_ = false;
    ++pin_generation_;
    failed_attempts_ = 0;
    lockout_until_ = QDateTime();
    // An explicit clear (re-auth path) — re-read rather than assume, so a
    // remove() that failed leaves the state Unreadable instead of "no PIN".
    retry_scheduled_ = false;
    const PinState before = pin_state_;
    load_state();
    if (pin_state_ != before)
        emit pin_state_changed();

    LOG_AUDIT("Auth", "PIN cleared");
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
    LOG_DEBUG("Auth", "PIN lockout reset after re-authentication");
    SecurityAuditLog::instance().record("lockout_cleared");
}

} // namespace fincept::auth
