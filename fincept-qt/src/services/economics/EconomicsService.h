// src/services/economics/EconomicsService.h
// Singleton service for all economics data sources.
// Wraps PythonRunner, caches results, emits result_ready.
#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept::services {

struct EconomicsResult {
    bool        success = false;
    QJsonObject data;
    QString     error;
    QString     source_id;  // which provider produced this
};

class EconomicsService : public QObject {
    Q_OBJECT
  public:
    static EconomicsService& instance();

    /// Run a script command asynchronously.
    /// @param source_id  caller tag echoed back in result_ready
    /// @param script     python script filename
    /// @param command    first CLI argument
    /// @param args       additional arguments
    /// @param request_id unique id to correlate response
    void execute(const QString& source_id,
                 const QString& script,
                 const QString& command,
                 const QStringList& args,
                 const QString& request_id);

    /// Invalidate cache for a given request key (force re-fetch).
    void invalidate(const QString& request_id);

  signals:
    void result_ready(const QString& request_id, const fincept::services::EconomicsResult& result);

  private:
    explicit EconomicsService(QObject* parent = nullptr);
    Q_DISABLE_COPY(EconomicsService)

    struct CacheEntry {
        QJsonObject data;
        qint64      fetched_at = 0;
    };
    QMap<QString, CacheEntry> cache_;
    static constexpr int kCacheTtlMs = 10 * 60 * 1000; // 10 min

    QString cache_key(const QString& script,
                      const QString& command,
                      const QStringList& args) const;
    bool    is_cache_fresh(const QString& key) const;
};

} // namespace fincept::services
