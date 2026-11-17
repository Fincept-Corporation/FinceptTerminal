#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct DataMapping {
    QString id;
    QString name;
    QString source_id;   // FK to data_sources.id (may be empty)
    QString schema_name; // OHLCV | QUOTE | TICK | ORDER | POSITION | PORTFOLIO | INSTRUMENT | TIMESERIES | MACRO | CUSTOM
    // ── v1 (pre-v062) request fields — still written for mappings with an inline HTTP source ──
    QString base_url;
    QString endpoint;
    QString method;    // GET | POST
    QString auth_type; // None | API Key | Bearer Token | Basic Auth
    QString auth_token; // kept in SecureStorage ("datamapping:auth:<id>"); the column holds ""
    /// The stored auth value could not be read (SecureStorage error, NOT "none stored"):
    /// auth_token is empty because it is unknown; save() keeps the stored copy.
    bool auth_unreadable = false;
    QString auth_read_error; // completes "Could not read secure storage: …"
    QString headers;
    QString body;
    QString parser; // JSONPath
    bool cache_enabled = true;
    int cache_ttl = 300;
    QString field_mappings_json; // v1: JSON array of {target, expression, transform, default_val}
    QString created_at;
    QString updated_at;
    // ── v2 (migration v062) ──
    QString spec_json;        ///< MappingSpec JSON; "" for a mapping never saved since v062
    int spec_version = 0;     ///< 0 = legacy (convert with spec_from_legacy), 2 = spec_json is authoritative
    int revision = 1;         ///< revision of spec_json (data_mapping_revisions keeps every one)
    QString last_run_at;
    QString last_run_status;  ///< "ok" | "issues" | "error" | ""
    int last_run_rows = 0;
    int last_run_errors = 0;
};

struct DataMappingRevision {
    int revision = 0;
    QString spec_json;
    QString saved_at;
};

/// The last output of a mapping run (what exports and the workflow hooks read).
struct MappedDataset {
    QString mapping_id;
    int revision = 0;
    QString schema_name;
    QString columns_json; // ["c1", …]
    QString types_json;   // ["number", …]
    QString rows_json;    // [[…], …]
    int row_count = 0;
    int error_cells = 0;
    QString source;
    QString produced_at;
};

class DataMappingRepository : public BaseRepository<DataMapping> {
  public:
    static DataMappingRepository& instance();

    Result<QVector<DataMapping>> list_all();
    Result<QVector<DataMapping>> list_by_source(const QString& source_id);
    Result<QVector<DataMapping>> list_by_schema(const QString& schema_name);
    Result<DataMapping> get(const QString& id);
    /// Insert / update the mapping row (no revision bookkeeping; v1 callers).
    Result<void> save(const DataMapping& dm);
    /// Save a v2 mapping as a new revision: bumps `revision`, stores spec_json in the row and in
    /// data_mapping_revisions. Returns the new revision.
    Result<int> save_revision(DataMapping dm);
    Result<QVector<DataMappingRevision>> list_revisions(const QString& mapping_id);
    Result<void> remove(const QString& id);
    Result<void> update_run_stats(const QString& id, const QString& status, int rows, int errors);

    Result<void> save_output(const MappedDataset& out);
    std::optional<MappedDataset> get_output(const QString& mapping_id);

  private:
    DataMappingRepository() = default;
    static DataMapping map_row(QSqlQuery& q);
};

} // namespace fincept
