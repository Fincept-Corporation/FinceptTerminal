// src/services/economics/EconomicsService.h
// Singleton service for all economics data sources.
// Wraps PythonRunner, caches results, emits result_ready.
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#    include "datahub/Producer.h"

namespace fincept::services {

struct EconomicsResult {
    bool success = false;
    QJsonObject data;
    QString error;
    QString source_id; // which provider produced this
};

/// Phase 6 — DataHub producer for `econ:<source>:<request_id>` topics.
/// The service is a thin Python-script dispatcher; hub topics are keyed
/// by (source_id, request_id) matching the legacy `result_ready` signal
/// tuple. Panels can migrate to `hub.subscribe("econ:fred:gdp_quarterly",
/// ...)` and the service fans out dual — hub publish + legacy signal —
/// so migration is incremental.
class EconomicsService : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static EconomicsService& instance();

    /// Run a script command asynchronously.
    /// @param source_id  caller tag echoed back in result_ready
    /// @param script     python script filename
    /// @param command    first CLI argument
    /// @param args       additional arguments
    /// @param request_id unique id to correlate response
    void execute(const QString& source_id, const QString& script, const QString& command, const QStringList& args,
                 const QString& request_id);

    /// Invalidate cache for a given request key (force re-fetch).
    void invalidate(const QString& request_id);

    /// Register with the hub + install econ:* policies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ────────────────────────────────────────
    /// Patterns: `econ:*`. Individual series topic shape is
    /// `econ:<source>:<request_id>` (request_id is caller-assigned and
    /// usually carries the series id or query hash).
    QStringList topic_patterns() const override;
    /// Split topic by `:` → source, request_id. If a pending dispatch
    /// record exists for that topic (registered via `remember_topic_*`),
    /// re-execute it. Otherwise no-op — scheduler-driven refresh without
    /// a prior execute() call is a no-op since we don't know the script.
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;  // 2 — Python spawn pacing

  signals:
    void result_ready(const QString& request_id, const fincept::services::EconomicsResult& result);

  private:
    explicit EconomicsService(QObject* parent = nullptr);
    Q_DISABLE_COPY(EconomicsService)

    static constexpr int kCacheTtlSec = 10 * 60; // 10 min — passed to CacheManager

    QString cache_key(const QString& script, const QString& command, const QStringList& args) const;

    /// Compose hub topic: `econ:<source>:<request_id>`.
    static QString hub_topic(const QString& source_id, const QString& request_id);
    /// Remember dispatch params so `refresh(topic)` can replay the call.
    struct DispatchRecord {
        QString source_id;
        QString script;
        QString command;
        QStringList args;
        QString request_id;
    };
    QHash<QString, DispatchRecord> dispatch_records_;  // topic -> last dispatch
    bool hub_registered_ = false;
};

} // namespace fincept::services

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::services::EconomicsResult)
