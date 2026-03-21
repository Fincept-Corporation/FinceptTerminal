#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct DataSource {
    QString id;
    QString alias;
    QString display_name;
    QString description;
    QString type;       // "websocket" | "rest_api"
    QString provider;
    QString category;
    QString config;     // JSON
    bool    enabled = true;
    QString tags;
    QString created_at;
    QString updated_at;
};

class DataSourceRepository : public BaseRepository<DataSource> {
public:
    static DataSourceRepository& instance();

    Result<QVector<DataSource>> list_all();
    Result<DataSource>          get(const QString& id);
    Result<void>                save(const DataSource& ds);
    Result<void>                remove(const QString& id);
    Result<void>                set_enabled(const QString& id, bool enabled);

private:
    DataSourceRepository() = default;
    static DataSource map_row(QSqlQuery& q);
};

} // namespace fincept
