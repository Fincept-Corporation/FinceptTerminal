#pragma once
#include "core/result/Result.h"

#include <QString>

namespace fincept {

/// Local credential store backed by SQLite + AES-256-GCM.
///
/// Values are encrypted with a 256-bit key derived once per process from
/// machine-local identifiers (`QSysInfo::machineUniqueId()` + a fixed app
/// salt, hashed with SHA-256). Each row carries its own random 96-bit IV
/// and 128-bit GCM authentication tag, so a tampered ciphertext fails
/// `retrieve()` rather than returning corrupted plaintext.
///
/// Threat model — what this protects against:
///   * Casual `grep` / inspection of the on-disk SQLite file.
///   * Profile copies moved to a different machine — the key derives from
///     the source machine's ID and won't decrypt elsewhere.
///   * Bit-flips or partial writes — GCM tag verification rejects them.
///
/// What this does NOT protect against:
///   * Another process running as the same user on the same machine — it
///     can read the DB file and re-derive the key from the same machine ID.
///     This matches the previous Linux fallback's threat model.
///   * Forensic tools that dump kernel memory while the app is running.
///   * Targeted malware that knows where to look.
///
/// Latency: AES-NI / ARM-Crypto-Extensions accelerate AES-GCM in hardware
/// on every supported CPU. Per-call cost is dominated by the SQLite write,
/// not the crypto — typically well under 1ms for the values we store
/// (PIN hash, API tokens, session IDs).
class SecureStorage {
  public:
    static SecureStorage& instance();

    Result<void> store(const QString& key, const QString& value);
    Result<QString> retrieve(const QString& key);
    Result<void> remove(const QString& key);

  private:
    SecureStorage() = default;
};

} // namespace fincept
