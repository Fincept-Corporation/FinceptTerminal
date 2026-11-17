#pragma once
// MappingRunner — Data Mapping's I/O: loads a mapping's source as a table (saved connection,
// catalogue connector, local file, or the inline HTTP request of a pre-v062 mapping), applies the
// MappingEngine off the GUI thread, stores the output (mapped_datasets, run stats, DataHub topic
// "mapping:<id>") and exports it as CSV, JSON or a SQLite table.
// All callbacks arrive on the GUI thread and are dropped when `context` is gone.

#include "services/data_mapping/MappingEngine.h"
#include "services/data_mapping/MappingSpec.h"
#include "services/data_sources/SourceError.h"
#include "services/data_sources/TabularData.h"
#include "storage/repositories/DataMappingRepository.h"

#include <QObject>
#include <QString>

#include <functional>

namespace fincept::services::datamapping {

struct SourceData {
    bool ok = false;
    fincept::services::datasources::SourceError error;
    fincept::services::datasources::TabularResult data;
    QStringList notes;
};

class MappingRunner : public QObject {
    Q_OBJECT
  public:
    using SourceCallback = std::function<void(const SourceData&)>;
    using MapCallback = std::function<void(const MappingOutput&)>;
    using DoneCallback = std::function<void(const fincept::services::datasources::SourceError&)>;

    static MappingRunner& instance();

    /// Fetch the source of `spec` (at most `limit` rows). `auth_token` = the legacy HTTP source's
    /// secret (from DataMappingRepository), never stored in the spec. Returns a job id for cancel().
    quint64 load_source(const SourceRef& source, const QString& auth_token, int limit, QObject* context,
                        SourceCallback cb);
    void cancel(quint64 job);

    /// Apply the spec on a worker thread (large tables never block the GUI).
    void apply_async(const fincept::services::datasources::DataTable& input, const MappingSpec& spec, int max_rows,
                     QObject* context, MapCallback cb);

    /// Persist a run: mapped_datasets row, run stats on the mapping, DataHub "mapping:<id>".
    void store_output(const QString& mapping_id, int revision, const QString& schema, const MappingOutput& out,
                      const QString& source_label);

    /// Exports (worker thread; `done` gets an empty error on success).
    void export_csv(const fincept::services::datasources::DataTable& table, const QString& path, QObject* context,
                    DoneCallback done);
    void export_json(const fincept::services::datasources::DataTable& table, const QString& path, QObject* context,
                     DoneCallback done);
    void export_sqlite(const fincept::services::datasources::DataTable& table, const QString& path,
                       const QString& table_name, QObject* context, DoneCallback done);

    /// The DataHub topic a mapping's runs are published on.
    static QString topic_for(const QString& mapping_id) { return QStringLiteral("mapping:") + mapping_id; }

  private:
    explicit MappingRunner(QObject* parent = nullptr);
    Q_DISABLE_COPY(MappingRunner)
    QHash<quint64, std::function<void()>> cancels_;
    quint64 next_ = 0;
};

} // namespace fincept::services::datamapping
