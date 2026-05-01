#pragma once
#include "core/result/Result.h"

#include <QDateTime>
#include <QObject>
#include <QString>

#include <array>

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
class PinManager : public QObject {
    Q_OBJECT
  public:
    static PinManager& instance();

    /// True if user has configured a PIN (hash exists in SecureStorage).
    bool has_pin() const;

    /// Set or update the PIN. Returns error if hash/storage fails or the PIN
    /// is trivially weak (all-same digits, ascending/descending sequence).
    /// Note: this does NOT require the old PIN — use change_pin() for that.
    Result<void> set_pin(const QString& pin);

    /// Change the PIN, requiring the current PIN for authorization. A wrong
    /// old_pin increments the failed-attempt counter exactly like a failed
    /// unlock, so an attacker who unlocks an unattended terminal cannot
    /// silently swap the PIN without facing the same lockout.
    Result<void> change_pin(const QString& old_pin, const QString& new_pin);

    /// Verify a PIN attempt. On failure increments attempt counter and
    /// may trigger lockout. Returns true on match.
    bool verify_pin(const QString& pin);

    /// Remove stored PIN (e.g. on logout or account reset).
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

    QByteArray derive_key(const QString& pin, const QByteArray& salt) const;

    int failed_attempts_ = 0;
    QDateTime lockout_until_;

    // Set transiently by change_pin() so the verify_pin call inside it can
    // tag audit-log entries with source=change_pin. False during normal
    // lock-screen unlocks. Reset to false after verify_pin returns.
    bool audit_source_change_pin_ = false;

    // Cached state loaded from SecureStorage
    bool has_pin_ = false;
    QByteArray stored_hash_;
    QByteArray stored_salt_;

    void load_state();
    void save_lockout_state();
    void load_lockout_state();
};

} // namespace fincept::auth
