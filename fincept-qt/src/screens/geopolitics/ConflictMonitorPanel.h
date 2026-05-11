// src/screens/geopolitics/ConflictMonitorPanel.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::ui {
class WorldMapWidget;
}

namespace fincept::screens {

/// Shows geopolitical events in a sortable table with category color coding
/// and an interactive world map plotting event locations.
class ConflictMonitorPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ConflictMonitorPanel(QWidget* parent = nullptr);
    void set_events(const QVector<fincept::services::geo::NewsEvent>& events);

  private:
    void build_ui();
    QWidget* build_sidebar();
    QWidget* build_overview_section(QWidget* parent);
    QWidget* build_top_categories_section(QWidget* parent);
    QWidget* build_hotspots_section(QWidget* parent);
    QWidget* build_event_details_section(QWidget* parent);
    QWidget* make_section_header(const QString& text, QWidget* parent);
    QWidget* make_divider(QWidget* parent);
    QWidget* make_stat_tile(const QString& label, QLabel** value_out, QWidget* parent);

    void update_stats(const QVector<fincept::services::geo::NewsEvent>& events);
    void update_hotspots(const QVector<fincept::services::geo::NewsEvent>& events);
    void update_map(const QVector<fincept::services::geo::NewsEvent>& events);

    // Map
    fincept::ui::WorldMapWidget* map_widget_ = nullptr;

    QTableWidget* events_table_ = nullptr;

    // Overview tiles
    QLabel* stat_total_ = nullptr;
    QLabel* stat_mapped_ = nullptr;
    QLabel* stat_countries_ = nullptr;

    // Top categories list
    QVBoxLayout* stats_layout_ = nullptr;

    // Hotspots (top countries) list
    QVBoxLayout* hotspots_layout_ = nullptr;

    // Selected event detail
    QLabel* detail_title_ = nullptr;
    QLabel* detail_category_ = nullptr;
    QLabel* detail_country_ = nullptr;
    QLabel* detail_city_ = nullptr;
    QLabel* detail_date_ = nullptr;
    QLabel* detail_source_ = nullptr;
    QPushButton* detail_open_btn_ = nullptr;
    QWidget* detail_panel_ = nullptr;
    QLabel* empty_state_ = nullptr;
    QString current_url_;
};

} // namespace fincept::screens
