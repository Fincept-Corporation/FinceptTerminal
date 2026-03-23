#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct Setting {
    QString key;
    QString value;
    QString category;
    QString updated_at;
};

/// Repository for the settings key-value table.
class SettingsRepository : public BaseRepository<Setting> {
  public:
    static SettingsRepository& instance();

    Result<void> set(const QString& key, const QString& value, const QString& category = "general");
    Result<QString> get(const QString& key, const QString& default_val = {});
    Result<void> remove(const QString& key);
    Result<QVector<Setting>> get_by_category(const QString& category);
    Result<void> clear_category(const QString& category);

  private:
    SettingsRepository() = default;
    static Setting map_row(QSqlQuery& q);
};

} // namespace fincept
