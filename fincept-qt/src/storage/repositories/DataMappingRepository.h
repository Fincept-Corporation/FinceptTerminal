#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct DataMapping {
    QString id;
    QString name;
    QString source_id;        // FK to data_sources.id (may be empty)
    QString schema_name;      // OHLCV | QUOTE | TICK | ORDER | POSITION | PORTFOLIO | INSTRUMENT
    QString base_url;
    QString endpoint;
    QString method;           // GET | POST
    QString auth_type;        // None | API Key | Bearer Token | Basic Auth
    QString auth_token;
    QString headers;
    QString body;
    QString parser;           // JSONPath
    bool cache_enabled = true;
    int cache_ttl = 300;
    QString field_mappings_json; // JSON array of {target, expression, transform, default_val}
    QString created_at;
    QString updated_at;
};

class DataMappingRepository : public BaseRepository<DataMapping> {
  public:
    static DataMappingRepository& instance();

    Result<QVector<DataMapping>> list_all();
    Result<QVector<DataMapping>> list_by_source(const QString& source_id);
    Result<QVector<DataMapping>> list_by_schema(const QString& schema_name);
    Result<DataMapping> get(const QString& id);
    Result<void> save(const DataMapping& dm);
    Result<void> remove(const QString& id);

  private:
    DataMappingRepository() = default;
    static DataMapping map_row(QSqlQuery& q);
};

} // namespace fincept
