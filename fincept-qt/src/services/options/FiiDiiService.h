#pragma once
// FiiDiiService — DataHub Producer for `fno:fii_dii:daily`.
//
// On `refresh()` (called by the hub scheduler when a subscriber wants fresh
// data), the service runs scripts/fii_dii_scraper.py via PythonRunner,
// upserts the result into `fii_dii_daily`, then publishes the last
// `kPublishWindow` days as `QVector<FiiDiiDay>` on the topic.
//
// Cadence
// ───────
// Topic policy: ttl_ms = 1 h, min_interval_ms = 30 min. NSE only refreshes
// the underlying numbers once per trading day (post 6 PM IST), so polling
// faster than that wastes upstream calls without changing data.

#include "datahub/Producer.h"
#include "services/options/FiiDiiTypes.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept::services::options {

class FiiDiiService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static FiiDiiService& instance();

    /// Idempotent — registers the topic, applies the policy, kicks off an
    /// initial refresh in the background so subscribers get data on first
    /// reveal without waiting a full scheduler cycle.
    void ensure_registered_with_hub();

    // ── Producer ────────────────────────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 1; }

  signals:
    void published(const QVector<fincept::services::options::FiiDiiDay>& rows);
    void failed(const QString& error);

  private:
    FiiDiiService();
    ~FiiDiiService() override = default;
    FiiDiiService(const FiiDiiService&) = delete;
    FiiDiiService& operator=(const FiiDiiService&) = delete;

    /// Spawn the scraper, parse stdout JSON, upsert, publish.
    void kick_off_refresh();

    bool registered_ = false;
    bool in_flight_ = false;
};

} // namespace fincept::services::options
