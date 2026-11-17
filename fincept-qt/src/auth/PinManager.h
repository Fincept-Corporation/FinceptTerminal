#pragma once
#include "core/result/Result.h"
#include "storage/secure/SecretRead.h"

#include <QDateTime>
#include <QObject>
#include <QString>

#include <array>
#include <deque>
#include <functional>
#include <memory>

class QThreadPool;

namespace fincept::auth {

/// Manages local PIN authentication — hashing, verification, and lockout.
///
/// PIN is hashed with PBKDF2-SHA256 (600k iterations, 32-byte salt) and stored
/// in OS-native SecureStorage (DPAPI / Keychain). Failure ladder (kFreeAttempts
/// = 2, kMaxAttempts = 5):
///   attempts 1-2 → silent grace period, "incorrect" message only
///   attempt 3   → 30s timed lockout
///   attempt 4   → 60s timed lockout
///   attempt 5   → permanent lockout, requires server re-authentication.
/// Total online keyspace exposure for a 6-digit PIN: 4 cheap guesses then the
/// re-auth gate. Two extra ladder tiers (5min / 15min) are deliberately *not*
/// reachable under the current kMaxAttempts — keeping them in code but
/// unreachable would be a foot-gun for future code that bumps the cap without
/// reading the comment.
///
/// Threading: the key derivation is deliberately slow (seconds on a busy
/// machine), so every operation that derives a key is asynchronous. The
/// derivation runs on a private worker pool; everything else — the lockout
/// checks, the attempt accounting, SecureStorage writes, signals and the
/// caller's callback — runs on the PinManager's thread (the GUI thread).
/// Operations are serialised: one runs at a time and each sees the lockout
/// state the previous one left, exactly like the old synchronous calls, so
/// parallel submissions (two windows, a repeated Enter) can neither skip the
/// lockout ladder nor be counted twice.
class PinManager : public QObject {
    Q_OBJECT
  public:
    static PinManager& instance();

    /// Callback of verify_pin_async(): true when the PIN matched.
    using VerifyCallback = std::function<void(bool ok)>;
    /// Callback of set_pin_async() / change_pin_async().
    using ResultCallback = std::function<void(Result<void>)>;

    /// What the stored PIN record says.
    ///   Set        — a PIN is configured (hash + salt read).
    ///   NotSet     — SecureStorage was read and holds no PIN.
    ///   Unreadable — SecureStorage could not be read (or the record is damaged).
    ///                NOT "no PIN": the terminal stays locked, no PIN may be set
    ///                over it, and the UI shows "Could not read secure storage:
    ///                <reason>. Retry" (WindowFrame). Transient errors are retried
    ///                automatically with back-off; reload() retries now.
    enum class PinState { Set, NotSet, Unreadable };
    PinState pin_state() const { return pin_state_; }
    /// Why the state is Unreadable (failure kind + plain-language reason()).
    const SecretRead& read_failure() const { return read_failure_; }

    /// Re-read the PIN record and lockout state from SecureStorage now (the
    /// blocked screen's Retry). Emits pin_state_changed() when the state changes.
    void reload();

    /// True unless SecureStorage was read and holds no PIN — i.e. also while
    /// the record is Unreadable, so every caller that decides "lock or not" on
    /// has_pin() fails closed. Use pin_state() to tell Set from Unreadable.
    bool has_pin() const;

    /// Set or update the PIN. Fails if hash/storage fails or the PIN is
    /// trivially weak (all-same digits, ascending/descending sequence).
    /// Note: this does NOT require the old PIN — use change_pin_async() for that.
    ///
    /// `done` runs on this object's thread once the PIN is stored (or refused),
    /// never synchronously inside this call. `context` scopes the callback: if
    /// it is destroyed first the callback is skipped, and an operation that has
    /// not started yet is dropped. The operation itself still completes once
    /// started — a PIN being stored is not abandoned halfway.
    void set_pin_async(const QString& pin, QObject* context, ResultCallback done);

    /// Change the PIN, requiring the current PIN for authorization. A wrong
    /// old_pin increments the failed-attempt counter exactly like a failed
    /// unlock, so an attacker who unlocks an unattended terminal cannot
    /// silently swap the PIN without facing the same lockout. Callback rules
    /// as for set_pin_async().
    void change_pin_async(const QString& old_pin, const QString& new_pin, QObject* context, ResultCallback done);

    /// Verify a PIN attempt. On failure increments attempt counter and may
    /// trigger lockout; the attempt is accounted (and lockout_changed /
    /// max_attempts_exceeded emitted) before `done` runs, and is accounted even
    /// if `context` is gone by then. Callback rules as for set_pin_async().
    void verify_pin_async(const QString& pin, QObject* context, VerifyCallback done);

    /// Drop the operations `context` queued that have not started yet; their
    /// callbacks never run and nothing is counted. An operation whose key
    /// derivation is already running finishes and is accounted as usual.
    void cancel_pending(QObject* context);

    /// True while an operation is queued or its key derivation is running.
    bool is_busy() const;

    /// Remove stored PIN (e.g. on logout or account reset). A verification
    /// whose derivation is still running when the PIN is cleared is discarded
    /// (reported as a mismatch, not counted).
    void clear_pin();

    /// Current lockout state.
    bool is_locked_out() const;

    /// Seconds remaining until lockout expires. 0 if not locked.
    int lockout_remaining_seconds() const;

    /// Number of consecutive failed attempts.
    int failed_attempts() const { return failed_attempts_; }

    /// Max attempts before permanent lockout (requires server re-auth).
    static constexpr int kMaxAttempts = 5;

    /// Number of "free" wrong attempts before the timed-lockout ladder kicks
    /// in. Set to 2 so the user gets 3 tries (attempts 1, 2, 3) with only a
    /// generic "incorrect" error; the 3rd mistake starts the 30s lockout.
    /// kMaxAttempts still caps the total at 5 before forced re-login.
    static constexpr int kFreeAttempts = 2;

    /// Reset lockout state (called after successful server re-auth).
    void reset_lockout();

  signals:
    /// Emitted when lockout state changes.
    void lockout_changed(bool locked, int remaining_seconds);

    /// Emitted when pin_state() changes (an automatic retry or reload() succeeded,
    /// or a read started failing).
    void pin_state_changed();

    /// Emitted when max attempts exceeded — requires full re-login.
    void max_attempts_exceeded();

  private:
    PinManager();

    // PBKDF2-SHA256 parameters.
    // 600_000 iterations per OWASP Password Storage Cheat Sheet (2023) for
    // PBKDF2-SHA256. Verify cost stays well under 500ms on target hardware.
    static constexpr int kIterations = 600000;
    static constexpr int kSaltLength = 32;
    static constexpr int kHashLength = 32;

    // Lockout durations (seconds) per attempt tier. Sized to match the
    // tiers that are actually reachable given kFreeAttempts + kMaxAttempts.
    // If you bump kMaxAttempts, also extend this array — see class header.
    static constexpr std::array<int, 2> kLockoutTiers = {30, 60};

    /// Pure function of its arguments — safe on any thread (it runs on the
    /// worker pool).
    static QByteArray derive_key(const QString& pin, const QByteArray& salt);

    struct Op;      // one queued set / change / verify request (PinManager.cpp)
    struct Derived; // what the worker hands back

    /// Run queued operations until one is waiting for its key derivation.
    void pump();
    void schedule_pump();
    /// Lockout gate of a verification (the old verify_pin() prologue). Returns
    /// false — after emitting max_attempts_exceeded where the old code did —
    /// when the attempt may not proceed.
    bool verify_allowed();
    /// Start the derivation for `op` on the worker pool.
    void start_derivation(const std::shared_ptr<Op>& op);
    void on_derived(const std::shared_ptr<Op>& op, const Derived& d);
    /// Account a finished verification (counter, lockout ladder, audit, signals).
    bool account_verify(bool match, const QString& source);
    /// Persist a freshly derived hash + salt as the PIN. Resets the ladder.
    /// The record is first written as ONE staged row (read back and compared) —
    /// from then on it is the PIN — and only then copied into the pin_hash /
    /// pin_salt rows; a failure part-way can no longer leave a new hash beside an
    /// old salt (a record no PIN unlocks).
    Result<void> store_pin(const QByteArray& hash, const QByteArray& salt);
    /// Copy a staged record into pin_hash / pin_salt (each verified), then delete
    /// the staged row. False (staged row kept, retried at the next load) on failure.
    bool commit_staged_pin(const QByteArray& hash, const QByteArray& salt);
    static void finish(const std::shared_ptr<Op>& op, bool ok, const Result<void>& result);
    QByteArray random_salt() const;

    int failed_attempts_ = 0;
    QDateTime lockout_until_;

    // Cached state loaded from SecureStorage
    PinState pin_state_ = PinState::NotSet;
    SecretRead read_failure_;
    int auto_retry_step_ = 0;    ///< back-off step of the automatic retry of a transient read failure
    bool retry_scheduled_ = false;
    bool has_pin_ = false;
    QByteArray stored_hash_;
    QByteArray stored_salt_;
    /// Bumped whenever the stored PIN changes (set / clear). A derivation that
    /// started under another generation compared against a PIN that is gone.
    quint64 pin_generation_ = 0;
    /// load_state() found a staged record and tried to commit it this session.
    bool staged_commit_attempted_ = false;

    std::deque<std::shared_ptr<Op>> queue_;
    std::shared_ptr<Op> running_;
    bool pump_scheduled_ = false;
    /// Private pool so a busy global pool (startup loads) never delays an
    /// unlock. Two threads: change_pin derives the old and the new key at once.
    QThreadPool* kdf_pool_ = nullptr;

    /// Read the PIN record + lockout state. Sets pin_state_ / read_failure_.
    void load_state();
    void save_lockout_state();
    /// False (with `failure` set) when the lockout state could not be read.
    bool load_lockout_state(SecretRead* failure);
    void schedule_auto_retry();
};

} // namespace fincept::auth
