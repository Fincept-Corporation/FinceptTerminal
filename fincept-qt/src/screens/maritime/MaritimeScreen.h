// src/screens/maritime/MaritimeScreen.h
#pragma once
#include "screens/common/IStatefulScreen.h"
#include "services/maritime/GeocodingTypes.h"
#include "services/maritime/MaritimePortTypes.h"
#include "services/maritime/MaritimeTypes.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QCompleter>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHash>
#include <QHideEvent>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QStringListModel>
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
    void changeEvent(QEvent* event) override;

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
    void on_places_found(QVector<services::maritime::GeoPlace> places, QString context);
    void on_places_error(const QString& context, const QString& message);

  private:
    void build_ui();
    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();
    QWidget* build_top_bar();
    QWidget* build_left_panel();
    QWidget* build_center_panel();
    QWidget* build_right_panel();
    QWidget* build_status_bar();
    void connect_service();
    void rebuild_routes_from_vessels(const QVector<services::maritime::VesselData>& vessels);
    void populate_routes_table();
    void update_intelligence(int region_total, int loaded);
    void update_credits(int remaining);
    void update_map(const QVector<services::maritime::VesselData>& vessels);
    void apply_theme();
    /// Re-issue the most recent load (bbox if one is set, else global sample).
    /// Shared by the manual refresh button and the auto-refresh timer.
    void do_refresh();
    void set_status(const QString& text, const ui::ColorToken& color);
    /// Issue a world-bbox search and flag the result for spatial downsampling
    /// so the map fans out instead of clustering on whatever bbox the user
    /// last typed.
    void load_global_sample();
    /// Fill the area-search spinners from a bbox and issue a vessel
    /// area-search for it. Shared by the place-search selection, the raw
    /// bbox button, and (later) the map-draw tool so they all funnel through
    /// one path.
    void run_area_search(double min_lat, double max_lat, double min_lng, double max_lng,
                         bool filter_to_shapes = false);
    /// Called when the user finalizes / clears drawn shapes on the map.
    /// Computes the union bbox of all shapes and loads vessels for it;
    /// on_vessels_loaded then filters the result to vessels actually inside
    /// any drawn shape. No shapes left = no-op.
    void on_shapes_changed();
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

    // Left panel — fleet stats (all computed from the loaded vessel set).
    QLabel* stat_vessels_ = nullptr;    // LOADED   — vessels rendered on the map
    QLabel* stat_displayed_ = nullptr;  // IN REGION — server-reported area total
    QLabel* stat_moving_ = nullptr;     // MOVING    — speed > 0.5 kn
    QLabel* stat_speed_ = nullptr;      // AVG SPEED — mean speed of moving vessels
    QLabel* stat_routes_ = nullptr;
    QLabel* stat_ports_ = nullptr;

    // Center
    fincept::ui::WorldMapWidget* map_widget_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;       // manual refresh
    QPushButton* auto_refresh_btn_ = nullptr;  // AUTO-refresh toggle (off by default)
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

    // Right panel - place search (Nominatim geocoder). Primary area-input UX:
    // user types a place name, picks a suggestion, its bbox fills the spinners
    // and triggers a vessel area-search.
    QLineEdit*    place_query_edit_ = nullptr;
    QTableWidget* place_table_      = nullptr;
    QLabel*       place_status_     = nullptr;
    QVector<services::maritime::GeoPlace> place_results_;
    // Live typeahead: debounced geocoder search feeds a completer popup.
    QCompleter*       place_completer_       = nullptr;
    QStringListModel* place_completer_model_ = nullptr;
    QTimer*           place_debounce_        = nullptr;

    // Right panel - area search (raw lat/long). Kept behind an "Advanced"
    // expander; place search / map-draw are the primary inputs now.
    QWidget*        advanced_area_box_ = nullptr;
    QDoubleSpinBox* area_min_lat_ = nullptr;
    QDoubleSpinBox* area_max_lat_ = nullptr;
    QDoubleSpinBox* area_min_lng_ = nullptr;
    QDoubleSpinBox* area_max_lng_ = nullptr;

    // Right panel - ports search (Wikidata + Marine Regions + OSM Overpass)
    QLineEdit*    ports_query_edit_ = nullptr;
    QTableWidget* ports_table_      = nullptr;
    QLabel*       ports_status_     = nullptr;
    QVector<services::maritime::PortRecord> port_results_;
    QCompleter*       ports_completer_       = nullptr;
    QStringListModel* ports_completer_model_ = nullptr;
    QTimer*           ports_debounce_        = nullptr;
    // Voyage endpoint port resolution: maps the expected PortsCatalog context
    // ("name:<port>") to the pin label to plot when its results arrive, so
    // these lookups don't leak into the user-facing ports table.
    QHash<QString, QString> voyage_port_ctx_;

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
    /// Set by on_shapes_changed(); consumed by on_vessels_loaded so the next
    /// batch is filtered to vessels lying inside the user's drawn shapes.
    bool filter_to_shapes_ = false;

    // ── Static-text widgets cached for retranslateUi ─────────────────────────
    // Left panel buttons + section titles + stat captions.
    QPushButton* global_btn_ = nullptr;
    QPushButton* load_btn_ = nullptr;
    QLabel* draw_title_ = nullptr;
    QPushButton* sq_btn_ = nullptr;
    QPushButton* ci_btn_ = nullptr;
    QPushButton* clr_btn_ = nullptr;
    QLabel* intel_title_ = nullptr;
    QLabel* routes_title_ = nullptr;
    QLabel* stat_vessels_cap_ = nullptr;
    QLabel* stat_displayed_cap_ = nullptr;
    QLabel* stat_moving_cap_ = nullptr;
    QLabel* stat_speed_cap_ = nullptr;
    QLabel* stat_routes_cap_ = nullptr;
    QLabel* stat_ports_cap_ = nullptr;
    // Center.
    QLabel* center_title_ = nullptr;
    QLabel* basemap_cap_ = nullptr;        // "BASEMAP" caption in the map toolbar
    QComboBox* map_type_combo_ = nullptr;  // basemap selector
    // Right panel buttons + titles + field captions.
    QLabel* search_title_ = nullptr;
    QLabel* imo_cap_ = nullptr;
    QPushButton* track_btn_ = nullptr;
    QPushButton* history_btn_ = nullptr;
    QLabel* place_title_ = nullptr;
    QLabel* place_cap_ = nullptr;
    QPushButton* place_btn_ = nullptr;
    QPushButton* area_toggle_ = nullptr;
    QLabel* coord_min_lat_cap_ = nullptr;
    QLabel* coord_max_lat_cap_ = nullptr;
    QLabel* coord_min_lng_cap_ = nullptr;
    QLabel* coord_max_lng_cap_ = nullptr;
    QPushButton* area_btn_ = nullptr;
    QLabel* ports_title_ = nullptr;
    QLabel* ports_cap_ = nullptr;
    QPushButton* ports_btn_ = nullptr;
    QPushButton* ports_in_view_btn_ = nullptr;
    QLabel* rd_title_ = nullptr;
    QLabel* classified_footer_ = nullptr;
    // Status bar captions + static value.
    QLabel* sb_source_cap_ = nullptr;
    QLabel* sb_records_cap_ = nullptr;
    QLabel* sb_refresh_cap_ = nullptr;
    QLabel* sb_refresh_val_ = nullptr;

    // Data
    QVector<services::maritime::TradeRoute> routes_;
    /// Vessels currently rendered as map pins. Indexed by MapPin::id so that
    /// pin_clicked can resolve a click back to the full record for display
    /// in the right-panel search-result card.
    QVector<services::maritime::VesselData> rendered_vessels_;
};

} // namespace fincept::screens
