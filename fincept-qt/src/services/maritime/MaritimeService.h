// src/services/maritime/MaritimeService.h
#pragma once
#include "services/maritime/MaritimeTypes.h"

#include <QObject>

namespace fincept::services::maritime {

/// Singleton service for Maritime Intelligence — HTTP calls to api.fincept.in/marine/
class MaritimeService : public QObject {
    Q_OBJECT
  public:
    static MaritimeService& instance();

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
    void vessels_loaded(QVector<fincept::services::maritime::VesselData> vessels, int total);
    void vessel_found(fincept::services::maritime::VesselData vessel);
    void vessel_history_loaded(QVector<fincept::services::maritime::VesselData> history);
    void health_loaded(QJsonObject data);
    void error_occurred(QString context, QString message);

  private:
    explicit MaritimeService(QObject* parent = nullptr);
    Q_DISABLE_COPY(MaritimeService)

    VesselData parse_vessel(const QJsonObject& obj) const;
};

} // namespace fincept::services::maritime
