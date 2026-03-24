// src/screens/geopolitics/ConflictMonitorPanel.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QLabel>
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
    void update_stats(const QVector<fincept::services::geo::NewsEvent>& events);
    void update_map(const QVector<fincept::services::geo::NewsEvent>& events);

    // Map
    fincept::ui::WorldMapWidget* map_widget_ = nullptr;

    QTableWidget* events_table_ = nullptr;
    QLabel* stats_label_ = nullptr;
    QVBoxLayout* stats_layout_ = nullptr;

    // Selected event detail
    QLabel* detail_category_ = nullptr;
    QLabel* detail_country_ = nullptr;
    QLabel* detail_city_ = nullptr;
    QLabel* detail_keywords_ = nullptr;
    QLabel* detail_date_ = nullptr;
    QLabel* detail_source_ = nullptr;
    QWidget* detail_panel_ = nullptr;
};

} // namespace fincept::screens
