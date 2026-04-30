// src/services/maritime/MaritimeService.h
#pragma once
#include "services/maritime/MaritimeTypes.h"

#include <QObject>

#    include "datahub/Producer.h"

namespace fincept::services::maritime {

/// Singleton service for Maritime Intelligence — HTTP calls to api.fincept.in/marine/
class MaritimeService : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static MaritimeService& instance();

    /// Register with the hub + install maritime:* policies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ─────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

    /// Search vessels in a geographic bounding box
    void search_vessels_by_area(const AreaSearchParams& params);

    /// Get single vessel position by IMO number
    void get_vessel_position(const QString& imo);

    /// Get multiple vessel positions by IMO array
    void get_multi_vessel_positions(const QStringList& imos);

    /// Get vessel history
    void get_vessel_history(const QString& imo);

    /// Health check for marine API module
    void check_health();

  signals:
    /// Emitted by both area-search and multi-vessel calls. The page envelope
    /// carries credit metering and (for multi-vessel) the list of IMOs the
    /// API could not resolve.
    void vessels_loaded(fincept::services::maritime::VesselsPage page);
    void vessel_found(fincept::services::maritime::VesselData vessel);
    void vessel_history_loaded(fincept::services::maritime::VesselHistoryPage page);
    void health_loaded(QJsonObject data);
    void error_occurred(QString context, QString message);

  private:
    explicit MaritimeService(QObject* parent = nullptr);
    Q_DISABLE_COPY(MaritimeService)

    VesselData parse_vessel(const QJsonObject& obj) const;

    bool hub_registered_ = false;
};

} // namespace fincept::services::maritime
