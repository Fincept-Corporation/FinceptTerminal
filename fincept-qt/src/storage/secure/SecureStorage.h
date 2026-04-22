#pragma once
#include "core/result/Result.h"

#include <QString>

namespace fincept {

/// Secure credential storage using OS-native backends:
///   Windows : Windows Credential Manager (DPAPI-encrypted, per-user DPAPI key)
///   macOS   : Security.framework Keychain (per-user Keychain, OS-protected)
///   Linux   : **XOR-obfuscated QSettings — NOT cryptographically secure.**
///             The obfuscation key is derived from machine-local identifiers
///             and stored alongside the data, so anyone with read access to
///             the user profile can recover every secret. This is a stopgap
///             that only stops casual `grep`-through-config inspection. It is
///             NOT safe against: another process running as the same user,
///             a stolen/cloned profile directory, forensic tools, or backups.
///             TODO: swap for a libsecret / KWallet / Secret Service backend
///             via a conditional build-time dependency. Track in issue
///             "Security: real Linux SecureStorage backend" (unfiled).
/// The PIN-manager PBKDF2 path still protects the PIN itself even on Linux —
/// but API keys, session tokens, and PIN-salt/hash stored here inherit only
/// the XOR obfuscation on that platform.
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
