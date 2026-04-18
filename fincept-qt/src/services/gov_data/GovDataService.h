// src/services/gov_data/GovDataService.h
#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

#    include "datahub/Producer.h"

namespace fincept::services {

// ── Provider metadata ────────────────────────────────────────────────────────

struct GovProviderInfo {
    QString id;
    QString name;
    QString full_name;
    QString description;
    QString color;
    QString country;
    QString flag;
    QString script; // Python script filename
};

// ── Generic result container ─────────────────────────────────────────────────

struct GovDataResult {
    bool success = false;
    QJsonObject data; // Full parsed JSON response
    QString error;
};

// ── Service ──────────────────────────────────────────────────────────────────

/// Phase 6 — DataHub producer for `govdata:<provider>:<request_id>`.
/// Same dispatch-replay shape as EconomicsService: execute() remembers
/// dispatch params so refresh(topic) can replay.
class GovDataService : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static GovDataService& instance();

    /// Run a government data command asynchronously.
    /// @param script   Python script filename (e.g. "government_us_data.py")
    /// @param command  Sub-command passed as first arg
    /// @param args     Additional arguments
    /// @param request_id  Caller-chosen id to match response
    void execute(const QString& script, const QString& command, const QStringList& args,
                 const QString& request_id = {});

    /// Get the static provider list
    static const QVector<GovProviderInfo>& providers();

    /// Find provider by id
    static const GovProviderInfo* provider_by_id(const QString& id);

    void ensure_registered_with_hub();
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;  // 2 — Python spawn pacing

  signals:
    void result_ready(const QString& request_id, const GovDataResult& result);

  private:
    explicit GovDataService(QObject* parent = nullptr);
    Q_DISABLE_COPY(GovDataService)

    static constexpr int kCacheTtlSec = 5 * 60;

    QString cache_key(const QString& script, const QString& command, const QStringList& args) const;

    /// Topic shape: `govdata:<provider>:<request_id>`. Provider is looked
    /// up from the script filename — falls back to the script name if no
    /// provider match.
    static QString hub_topic(const QString& script, const QString& request_id);
    struct DispatchRecord {
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
Q_DECLARE_METATYPE(fincept::services::GovDataResult)
