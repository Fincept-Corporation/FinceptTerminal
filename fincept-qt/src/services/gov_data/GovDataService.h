// src/services/gov_data/GovDataService.h
#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

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
    QJsonObject data;     // Full parsed JSON response
    QString error;
};

// ── Service ──────────────────────────────────────────────────────────────────

class GovDataService : public QObject {
    Q_OBJECT
  public:
    static GovDataService& instance();

    /// Run a government data command asynchronously.
    /// @param script   Python script filename (e.g. "government_us_data.py")
    /// @param command  Sub-command passed as first arg
    /// @param args     Additional arguments
    /// @param request_id  Caller-chosen id to match response
    void execute(const QString& script, const QString& command,
                 const QStringList& args, const QString& request_id = {});

    /// Get the static provider list
    static const QVector<GovProviderInfo>& providers();

    /// Find provider by id
    static const GovProviderInfo* provider_by_id(const QString& id);

  signals:
    void result_ready(const QString& request_id, const GovDataResult& result);

  private:
    explicit GovDataService(QObject* parent = nullptr);
    Q_DISABLE_COPY(GovDataService)

    // Simple TTL cache: key = "script:command:args_hash"
    struct CacheEntry {
        QJsonObject data;
        qint64 fetched_at = 0;
    };
    QMap<QString, CacheEntry> cache_;
    static constexpr int kCacheTtlMs = 5 * 60 * 1000; // 5 min

    bool is_cache_fresh(const QString& key) const;
    QString cache_key(const QString& script, const QString& command, const QStringList& args) const;
};

} // namespace fincept::services
