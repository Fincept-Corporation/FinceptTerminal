// src/screens/maritime/MaritimeScreen.h
#pragma once
#include "screens/IStatefulScreen.h"
#include "services/maritime/MaritimePortTypes.h"
#include "services/maritime/MaritimeTypes.h"
#include "ui/theme/Theme.h"

#include <QDoubleSpinBox>
#include <QHideEvent>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::ui {
class LoadingOverlay;
class WorldMapWidget;
}

namespace fincept::screens {

/// Maritime Intelligence screen — vessel tracking, trade routes, search.
/// Layout: Left stats panel | Center content (tabs) | Right detail panel
class MaritimeScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit MaritimeScreen(QWidget* parent = nullptr);

    // IStatefulScreen — persists the IMO search text and the area-search
    // bounds (common user inputs for vessel lookup).
    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return "maritime"; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_load_vessels();
    void on_search_vessel();
    void on_vessels_loaded(services::maritime::VesselsPage page);
    void on_vessel_found(services::maritime::VesselData vessel);
    void on_vessel_history(services::maritime::VesselHistoryPage page);
    void on_health_loaded(QJsonObject data);
    void on_error(const QString& context, const QString& message);
    void on_route_selected(int row);
    void on_ports_found(QVector<services::maritime::PortRecord> ports, QString context);
    void on_ports_error(const QString& context, const QString& message);

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_left_panel();
    QWidget* build_center_panel();
    QWidget* build_right_panel();
    QWidget* build_status_bar();
    void connect_service();
    void rebuild_routes_from_vessels(const QVector<services::maritime::VesselData>& vessels);
    void populate_routes_table();
    void update_intelligence(int vessel_count);
    void update_credits(int remaining);
    void update_map(const QVector<services::maritime::VesselData>& vessels);
    void apply_theme();
    void set_status(const QString& text, const ui::ColorToken& color);
    /// Issue a world-bbox search and flag the result for spatial downsampling
    /// so the map fans out instead of clustering on whatever bbox the user
    /// last typed.
    void load_global_sample();
    void show_map_loading(const QString& msg);
    void hide_map_loading();

    // Top bar widgets (needed for apply_theme)
    QWidget* top_bar_ = nullptr;
    QLabel* brand_label_ = nullptr;
    QLabel* classified_label_ = nullptr;
    QLabel* credits_label_ = nullptr;
    QLabel* not_found_label_ = nullptr;  // shown when multi-vessel returns missing IMOs

    // Left / right panel roots
    QWidget* left_panel_ = nullptr;
    QWidget* right_panel_ = nullptr;
    QWidget* status_bar_ = nullptr;

    // Left panel
    QLabel* stat_vessels_ = nullptr;
    QLabel* stat_displayed_ = nullptr;
    QLabel* stat_routes_ = nullptr;
    QLabel* stat_ports_ = nullptr;

    // Center
    fincept::ui::WorldMapWidget* map_widget_ = nullptr;
    fincept::ui::LoadingOverlay* map_loader_ = nullptr;
    QTableWidget* vessels_table_ = nullptr;
    QTableWidget* routes_table_ = nullptr;

    // Right panel - search
    QLineEdit* imo_edit_ = nullptr;
    QLabel* search_result_label_ = nullptr;
    QWidget* search_result_card_ = nullptr;
    QLabel* sr_name_ = nullptr;
    QLabel* sr_imo_ = nullptr;
    QLabel* sr_position_ = nullptr;
    QLabel* sr_speed_ = nullptr;
    QLabel* sr_from_ = nullptr;
    QLabel* sr_to_ = nullptr;

    // Right panel - area search
    QDoubleSpinBox* area_min_lat_ = nullptr;
    QDoubleSpinBox* area_max_lat_ = nullptr;
    QDoubleSpinBox* area_min_lng_ = nullptr;
    QDoubleSpinBox* area_max_lng_ = nullptr;

    // Right panel - ports search (Wikidata + Marine Regions + OSM Overpass)
    QLineEdit*    ports_query_edit_ = nullptr;
    QTableWidget* ports_table_      = nullptr;
    QLabel*       ports_status_     = nullptr;
    QVector<services::maritime::PortRecord> port_results_;

    // Route detail
    QWidget* route_detail_ = nullptr;
    QLabel* rd_name_ = nullptr;
    QLabel* rd_value_ = nullptr;
    QLabel* rd_status_ = nullptr;
    QLabel* rd_vessels_ = nullptr;

    // Status
    QLabel* status_label_ = nullptr;
    QLabel* vessel_count_label_ = nullptr;
    QLabel* source_value_ = nullptr;     // status-bar SOURCE value (driven by /marine/health)
    QLabel* records_value_ = nullptr;    // status-bar RECORDS value (db.total_records)

    // Timer
    QTimer* refresh_timer_ = nullptr;
    bool first_show_ = true;
    /// Set by load_global_sample(); consumed by on_vessels_loaded so that
    /// the next batch is spatially downsampled to ~200 globally-spread pins.
    bool pending_global_sample_ = false;

    // Data
    QVector<services::maritime::TradeRoute> routes_;
    /// Vessels currently rendered as map pins. Indexed by MapPin::id so that
    /// pin_clicked can resolve a click back to the full record for display
    /// in the right-panel search-result card.
    QVector<services::maritime::VesselData> rendered_vessels_;
};

} // namespace fincept::screens
