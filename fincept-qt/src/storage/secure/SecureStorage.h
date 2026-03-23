#pragma once
#include "core/result/Result.h"

#include <QString>

namespace fincept {

/// Secure credential storage using OS keychain (Windows Credential Manager).
/// Falls back to encrypted QSettings if keychain unavailable.
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
