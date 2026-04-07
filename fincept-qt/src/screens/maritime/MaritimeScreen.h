// src/screens/maritime/MaritimeScreen.h
#pragma once
#include "services/maritime/MaritimeTypes.h"
#include "ui/theme/Theme.h"

#include <QDoubleSpinBox>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::ui {
class WorldMapWidget;
}

namespace fincept::screens {

/// Maritime Intelligence screen — vessel tracking, trade routes, search.
/// Layout: Left stats panel | Center content (tabs) | Right detail panel
class MaritimeScreen : public QWidget {
    Q_OBJECT
  public:
    explicit MaritimeScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_load_vessels();
    void on_search_vessel();
    void on_vessels_loaded(QVector<services::maritime::VesselData> vessels, int total);
    void on_vessel_found(services::maritime::VesselData vessel);
    void on_vessel_history(QVector<services::maritime::VesselData> history);
    void on_error(const QString& context, const QString& message);
    void on_route_selected(int row);

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_left_panel();
    QWidget* build_center_panel();
    QWidget* build_right_panel();
    QWidget* build_status_bar();
    void connect_service();
    void populate_routes_table();
    void update_intelligence(int vessel_count);
    void update_map(const QVector<services::maritime::VesselData>& vessels);
    void apply_theme();
    void set_status(const QString& text, const ui::ColorToken& color);

    // Top bar widgets (needed for apply_theme)
    QWidget* top_bar_         = nullptr;
    QLabel*  brand_label_     = nullptr;
    QLabel*  classified_label_= nullptr;

    // Left / right panel roots
    QWidget* left_panel_  = nullptr;
    QWidget* right_panel_ = nullptr;
    QWidget* status_bar_  = nullptr;

    // Left panel
    QLabel* stat_vessels_  = nullptr;
    QLabel* stat_displayed_= nullptr;
    QLabel* stat_routes_   = nullptr;
    QLabel* stat_volume_   = nullptr;
    QLabel* stat_ports_    = nullptr;
    QLabel* threat_badge_  = nullptr;

    // Center
    fincept::ui::WorldMapWidget* map_widget_   = nullptr;
    QTableWidget*                vessels_table_= nullptr;
    QTableWidget*                routes_table_ = nullptr;

    // Right panel - search
    QLineEdit* imo_edit_           = nullptr;
    QLabel*    search_result_label_= nullptr;
    QWidget*   search_result_card_ = nullptr;
    QLabel*    sr_name_    = nullptr;
    QLabel*    sr_imo_     = nullptr;
    QLabel*    sr_position_= nullptr;
    QLabel*    sr_speed_   = nullptr;
    QLabel*    sr_from_    = nullptr;
    QLabel*    sr_to_      = nullptr;

    // Right panel - area search
    QDoubleSpinBox* area_min_lat_ = nullptr;
    QDoubleSpinBox* area_max_lat_ = nullptr;
    QDoubleSpinBox* area_min_lng_ = nullptr;
    QDoubleSpinBox* area_max_lng_ = nullptr;

    // Route detail
    QWidget* route_detail_ = nullptr;
    QLabel*  rd_name_      = nullptr;
    QLabel*  rd_value_     = nullptr;
    QLabel*  rd_status_    = nullptr;
    QLabel*  rd_vessels_   = nullptr;

    // Status
    QLabel* status_label_      = nullptr;
    QLabel* vessel_count_label_= nullptr;

    // Timer
    QTimer* refresh_timer_ = nullptr;
    bool    first_show_    = true;

    // Data
    QVector<services::maritime::TradeRoute> routes_;
};

} // namespace fincept::screens
