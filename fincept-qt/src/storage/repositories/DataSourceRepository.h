#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct DataSource {
    QString id;
    QString alias;
    QString display_name;
    QString description;
    QString type; // "websocket" | "rest_api"
    QString provider;
    QString category;
    QString config; // JSON
    bool enabled = true;
    QString tags;
    QString created_at;
    QString updated_at;
    // Result of the last real TEST (migration v062). Written only by update_health().
    QString last_status;      ///< "ok" | "error" | "" (never tested)
    QString last_code;        ///< connector-contract error code when last_status == "error"
    QString last_message;     ///< plain summary or failure message (secrets redacted)
    QString last_tested_at;   ///< UTC "yyyy-MM-dd HH:mm:ss"
    qint64 last_latency_ms = -1;
    /// The stored credentials could not be read (SecureStorage error, NOT "none stored"): the
    /// secret fields of `config` are blank because they are unknown. save() then keeps the
    /// stored copy (or refuses to replace it) instead of treating the blanks as cleared.
    bool secrets_unreadable = false;
    QString secrets_error{};  ///< plain reason, completing "Could not read secure storage: …"
};

class DataSourceRepository : public BaseRepository<DataSource> {
  public:
    static DataSourceRepository& instance();

    Result<QVector<DataSource>> list_all();
    Result<DataSource> get(const QString& id);
    /// Insert / update. Fails without writing anything when the connection's stored credentials
    /// could not be read when it was loaded and still cannot be (see DataSource::secrets_unreadable).
    Result<void> save(const DataSource& ds);
    Result<void> remove(const QString& id);
    Result<void> set_enabled(const QString& id, bool enabled);
    /// Record the outcome of a TEST. Safe from any thread (per-thread DB connection).
    Result<void> update_health(const QString& id, const QString& status, const QString& code, const QString& message,
                               qint64 latency_ms);

  private:
    DataSourceRepository() = default;
    static DataSource map_row(QSqlQuery& q);
};

} // namespace fincept
