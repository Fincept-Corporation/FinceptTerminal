#pragma once
#include "core/result/Result.h"
#include "storage/secure/SecretRead.h"

#include <QString>

#include <memory>

namespace fincept {

/// Local credential store backed by SQLite + AES-256-GCM.
///
/// Values are encrypted with a random 256-bit data-encryption key (DEK). The
/// DEK is stored wrapped: by DPAPI on Windows (user scope), by PBKDF2 over a
/// machine-local seed elsewhere (see SecureStorageSqlite.cpp). Each row
/// carries its own random 96-bit IV and 128-bit GCM tag, so a tampered or
/// damaged row is reported as Corrupt rather than returned as garbage.
///
/// Threat model — what this protects against:
///   * Casual `grep` / inspection of the on-disk SQLite file.
///   * Profile copies moved to another machine or (Windows) user account.
///   * Bit-flips or partial writes — GCM tag verification rejects them.
/// What it does NOT protect against: another process of the same user on the
/// same machine (non-Windows: it can re-derive the PBKDF2 key), kernel memory
/// dumps, targeted malware.
///
/// READING: use read(). It distinguishes "nothing stored" from "could not
/// read" (SecretRead.h). A caller must never treat an error as "nothing
/// stored": re-running a setup, writing a fresh secret over one it could not
/// read, or skipping a lock screen on that basis are all defects. Transient
/// errors (busy database, store not open yet) may be retried with back-off;
/// permanent ones (key unavailable, damaged row) need the user.
///
/// retrieve() is the older form: Ok(value) when found; an error whose text is
/// exactly kNotFound when nothing is stored; any other error text when the
/// store could not be read (is_not_found() tells them apart).
class SecureStorage {
  public:
    static SecureStorage& instance();

    /// Error text of retrieve() when nothing is stored under the key.
    static constexpr const char* kNotFound = "Not found";

    /// Tri-state read. Logs a failure once per key and kind (never the value).
    SecretRead read(const QString& key);

    Result<void> store(const QString& key, const QString& value);

    /// store(), then read the value back and compare: Ok only when the store now
    /// demonstrably holds `value`. Required before deleting or blanking any other
    /// copy of the secret (a plaintext row, a legacy slot) — see SecretMigration.h.
    Result<void> store_verified(const QString& key, const QString& value);

    Result<QString> retrieve(const QString& key);
    Result<void> remove(const QString& key);

    /// True when `r` failed because nothing is stored (as opposed to a read error).
    static bool is_not_found(const Result<QString>& r) {
        return r.is_err() && r.error() == std::string(kNotFound);
    }

    /// Erase every stored credential AND the wrapped key, so the store works
    /// again after a permanent KeyUnavailable error. Saved API keys, the
    /// session and the PIN are gone afterwards. Only on an explicit, confirmed
    /// user action.
    Result<void> reset_all();

    /// Bumped on every successful store() / remove() / reset_all(): lets a
    /// cache of decrypted values (the Python child environment) know it is stale.
    quint64 generation() const;

    /// Tests: replace the backend (nullptr restores the default on next use).
    void set_backend_for_testing(std::unique_ptr<ISecretBackend> backend);

  private:
    SecureStorage() = default;
    ISecretBackend& backend();
};

/// The production backend (SQLite + AES-256-GCM). Defined in
/// SecureStorageSqlite.cpp; a unit test that links only SecureStorage.cpp
/// defines its own.
std::unique_ptr<ISecretBackend> make_default_secret_backend();

} // namespace fincept
