#pragma once
// TEST STUB — shadows src/storage/repositories/SettingsRepository.h for widget test
// targets only (tests/CMakeLists.txt puts tests/stubs first on their include path).
// ThemeManager persists the direction palette through SettingsRepository; the real one
// needs the whole SQLite storage layer, which a widget's keyboard test has no use for.
// An in-memory key/value map with the same signatures ThemeManager calls.

#include "core/result/Result.h"

#include <QHash>
#include <QString>

namespace fincept {

class SettingsRepository {
  public:
    static SettingsRepository& instance() {
        static SettingsRepository s;
        return s;
    }
    Result<void> set(const QString& key, const QString& value, const QString& = QStringLiteral("general")) {
        values_.insert(key, value);
        return Result<void>::ok();
    }
    Result<QString> get(const QString& key, const QString& default_val = {}) {
        return Result<QString>::ok(values_.value(key, default_val));
    }
    Result<void> remove(const QString& key) {
        values_.remove(key);
        return Result<void>::ok();
    }

  private:
    SettingsRepository() = default;
    QHash<QString, QString> values_;
};

} // namespace fincept
