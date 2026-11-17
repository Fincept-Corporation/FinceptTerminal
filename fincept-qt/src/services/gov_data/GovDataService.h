// src/services/gov_data/GovDataService.h
#pragma once

#include "datahub/Producer.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>
#include <memory>

namespace fincept::services {

// ── Provider metadata ────────────────────────────────────────────────────────

struct GovProviderInfo {
    QString id;
    QString name;
    QString full_name;
    QString description;
    QString color; // identity colour for the dark theme — draw through ui::colors::legible() / tint()
    QString country;
    QString flag;
    QString script; // Python script filename
};

// ── Generic result container ─────────────────────────────────────────────────

struct GovDataResult {
    bool success = false;
    QJsonObject data; // Full parsed JSON response
    QString error;
    /// Why a request failed, when it can be told (empty = other / unknown):
    /// "missing_key", "invalid_key", "rate_limited", "timeout", "unreachable", "no_data",
    /// "not_supported", "bad_args", "cancelled".
    QString error_code;
    /// error_code "missing_key" / "invalid_key": the credential concerned ("CONGRESS_GOV_API_KEY"), when named.
    QString missing_key;
    /// error_code "rate_limited": seconds the source asked to wait (0 = not said).
    int retry_after_s = 0;
};

// ── Service ──────────────────────────────────────────────────────────────────

/// Phase 6 — DataHub producer for `govdata:<provider>:<request_id>`.
/// Same dispatch-replay shape as EconomicsService: execute() remembers
/// dispatch params so refresh(topic) can replay.
class GovDataService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    using CancelToken = std::shared_ptr<std::atomic<bool>>;

    static GovDataService& instance();

    /// Run a government data command asynchronously.
    /// @param script   Python script filename (e.g. "government_us_data.py")
    /// @param command  Sub-command passed as first arg
    /// @param args     Additional arguments
    /// @param request_id  Caller-chosen id to match response. A per-run suffix after '#'
    ///                    ("gov.treasury#17") keeps the hub topic per caller, not per run.
    /// @param cancel   Optional: raising it stops a queued or running script; the reply is then
    ///                 a failure with error_code "cancelled".
    void execute(const QString& script, const QString& command, const QStringList& args,
                 const QString& request_id = {}, const CancelToken& cancel = {});

    /// Get the static provider list
    static const QVector<GovProviderInfo>& providers();

    /// Find provider by id
    static const GovProviderInfo* provider_by_id(const QString& id);

    void ensure_registered_with_hub();
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override; // 2 — Python spawn pacing

  signals:
    void result_ready(const QString& request_id, const GovDataResult& result);

  private:
    explicit GovDataService(QObject* parent = nullptr);
    Q_DISABLE_COPY(GovDataService)

    static constexpr int kCacheTtlSec = 5 * 60;

    QString cache_key(const QString& script, const QString& command, const QStringList& args) const;

    /// Topic shape: `govdata:<provider>:<request_id without its '#run' suffix>`. Provider is
    /// looked up from the script filename — falls back to the script name if no provider match.
    static QString hub_topic(const QString& script, const QString& request_id);
    struct DispatchRecord {
        QString script;
        QString command;
        QStringList args;
        QString request_id;
    };
    QHash<QString, DispatchRecord> dispatch_records_; // topic -> last dispatch
    bool hub_registered_ = false;
};

} // namespace fincept::services

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::services::GovDataResult)
