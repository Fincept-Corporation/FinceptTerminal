#pragma once
#include "core/result/Result.h"

#include <QString>

namespace fincept {

/// Secure credential storage using OS-native backends:
///   Windows : Windows Credential Manager (DPAPI-encrypted)
///   macOS   : Security.framework Keychain
///   Linux   : XOR-obfuscated QSettings (NOT cryptographically secure —
///             prevents casual inspection; libsecret backend planned)
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
