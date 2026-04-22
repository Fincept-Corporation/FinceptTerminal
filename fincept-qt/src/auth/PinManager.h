#pragma once
#include "core/result/Result.h"

#include <QDateTime>
#include <QObject>
#include <QString>

#include <array>

namespace fincept::auth {

/// Manages local PIN authentication — hashing, verification, and lockout.
///
/// PIN is hashed with PBKDF2-SHA256 (100k iterations, 32-byte salt) and stored
/// in OS-native SecureStorage (DPAPI / Keychain). A lockout mechanism enforces
/// exponential backoff after failed attempts (30s, 60s, 5min, 15min, then
/// requires full server re-authentication).
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

    // Lockout durations (seconds) per attempt tier
    static constexpr std::array<int, 4> kLockoutTiers = {30, 60, 300, 900};

    QByteArray derive_key(const QString& pin, const QByteArray& salt) const;

    int failed_attempts_ = 0;
    QDateTime lockout_until_;

    // Cached state loaded from SecureStorage
    bool has_pin_ = false;
    QByteArray stored_hash_;
    QByteArray stored_salt_;

    void load_state();
    void save_lockout_state();
    void load_lockout_state();
};

} // namespace fincept::auth
