// src/screens/maritime/MaritimeScreen.h
#pragma once
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/maritime/GeocodingTypes.h"
#include "services/maritime/MaritimePortTypes.h"
#include "services/maritime/MaritimeTypes.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QJsonValue>
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

class QStackedWidget;

namespace fincept::ui {
class AutoLoadPane;
class Card;
class EmptyState;
class KpiStrip;
class LoadingOverlay;
class RangeBar;
class TimeSeriesChart;
class WorldMapWidget;
} // namespace fincept::ui

namespace fincept::screens {

/// Maritime Intelligence screen — vessel tracking, trade routes, search.
/// Layout: Left stats panel | Center content (tabs) | Right detail panel
class MaritimeScreen : public QWidget, public IStatefulScreen, public fincept::screens::ISectionLinked {
    Q_OBJECT
  public:
    explicit MaritimeScreen(QWidget* parent = nullptr);

    // ISectionLinked — no tabs; the right column's searches: "vessel" (IMO box), "places"
    // (shipping-area search), "ports" (port search): scrolled into view and focused. `focus` = an
    // IMO number for "vessel" (or with an empty section): typed into the box, never looked up.
    bool open_section(const QString& section, const QString& focus) override;

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
    /// Show the API credit balance (fractional); PLANS when low. < 0 = not reported.
    void update_credits(double remaining);
    /// `fit_camera` re-frames the map around the new pins; false keeps the user's pan/zoom.
    /// `update_stats` = false leaves the left-panel fleet stats alone (corridor subsets).
    void update_map(const QVector<services::maritime::VesselData>& vessels, bool fit_camera,
                    bool update_stats = true);
    /// Fill vessels_table_ with the first `limit` fleet rows (Progress column).
    void fill_fleet_table(const QVector<services::maritime::VesselData>& vessels, int limit);
    /// Switch vessels_table_ between the fleet columns and the voyage-history
    /// columns (Name / IMO hidden, last column = report time).
    void set_history_columns(bool history);
    /// Drop a corridor filter: the whole loaded fleet back in the table and on the map.
    void clear_route_filter();
    void apply_theme();
    /// Item-brush colours of the vessel names (the theme_changed part of the theme).
    void recolor_vessel_names();
    /// First show: the vessel-list view with a kept result (saved bbox, else
    /// global), else the saved bbox / global view's preview. Never a request.
    void open_initial_view();
    /// Fill the right-panel vessel card (name, IMO, fix + its age, speed, route) from a
    /// record and copy its IMO into the search box so VOYAGE HISTORY is one click away.
    void show_vessel_card(const services::maritime::VesselData& v);
    /// Table row -> vessel card (and fly the map there when `fly`). A voyage-history
    /// row flies the map to that fix.
    void on_vessel_row_picked(int row, bool fly);
    /// Read + validate the IMO box. Strips an "IMO" prefix; IMO numbers are exactly 7
    /// digits, so anything else is reported instead of burning an API call on it.
    /// Returns false (silently for an empty box) when there is nothing valid to look up.
    bool read_imo(QString* imo_out);
    /// Re-issue the most recent load (bbox if one is set, else global sample).
    /// Shared by the manual refresh button and the auto-refresh timer.
    void do_refresh();
    /// `tone`: "positive" / "amber" / "negative" / "muted" (root-sheet colour rule).
    void set_status(const QString& text, const char* tone);
    /// The global view: a world-bbox search whose result is spatially
    /// downsampled so the map fans out. Selects that pane view and runs it.
    void load_global_sample();
    /// Fill the area-search spinners from a bbox, select that pane view and run
    /// the vessel area-search for it. Shared by the place-search selection, the
    /// raw bbox button and the map-draw tool so they all funnel through one path.
    void run_area_search(double min_lat, double max_lat, double min_lng, double max_lng, bool filter_to_shapes = false);
    /// Built-in key shipping areas / major container ports in the place and
    /// port tables until the user searches (free, no request).
    void fill_default_places();
    void fill_default_ports();

    // ── The vessel list: the shared AutoLoadPane (Policy::Manual) ────────────
    // An area search costs 8 credits (voyage history 5): the pane never runs by
    // itself. Each view (global / a bbox / drawn shapes / one IMO's history) has
    // its remembered result; for an area that is the service's own kept copy
    // (remembered_vessels → MaritimeService::area_cached_at, parsed off-thread on
    // render) — nothing is duplicated. The map, corridors and fleet figures keep
    // following the service's vessels_loaded signal (apply_vessels_page).
    struct MtView {
        services::maritime::AreaSearchParams params;
        bool global = false;
        bool shapes = false;
        QString imo; // history view
    };
    QHash<QString, MtView> views_;
    /// Register `params` as a view, configure its preview and switch the pane to it.
    QString select_area_view(const services::maritime::AreaSearchParams& params, bool global, bool shapes);
    void configure_vessels_preview(const MtView& v);
    void start_vessels_request(quint64 id);
    void render_vessels_payload(const QJsonValue& payload, bool from_cache);
    bool remembered_vessels(const QString& view, QJsonValue* payload, QDateTime* as_of);
    /// Fill table, map, corridors and fleet figures from an area page.
    void apply_vessels_page(services::maritime::VesselsPage page, bool global_sample);
    fincept::ui::AutoLoadPane* vessels_pane_ = nullptr;
    QLabel* vessels_asof_lbl_ = nullptr; // map toolbar: "as of 14:02" (kept by the pane)
    quint64 vessels_request_id_ = 0;     // the pane run the outstanding request answers
    QString expected_area_key_;          // MaritimeService::area_request_key of that request
    QString applied_view_key_;           // the view whose vessels the table / map show
    quint64 cached_load_gen_ = 0;        // a kept copy still being parsed lands only if unchanged
    bool layers_defaulted_ = false;
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
    QLabel* credits_label_ = nullptr;
    QPushButton* plans_btn_ = nullptr; // Plans & Pricing — shown when credits run low / out
    QLabel* not_found_label_ = nullptr; // shown when multi-vessel returns missing IMOs

    // Left / right panel roots
    QWidget* left_panel_ = nullptr;
    QWidget* right_panel_ = nullptr;
    QWidget* status_bar_ = nullptr;

    // Left panel — fleet stats (all computed from the loaded vessel set).
    QLabel* stat_vessels_ = nullptr;   // LOADED   — vessels rendered on the map
    QLabel* stat_displayed_ = nullptr; // IN REGION — server-reported area total
    QLabel* stat_moving_ = nullptr;    // MOVING    — speed > 0.5 kn
    QLabel* stat_speed_ = nullptr;     // AVG SPEED — mean speed of moving vessels
    QLabel* stat_routes_ = nullptr;
    QLabel* stat_ports_ = nullptr;

    // Center
    fincept::ui::WorldMapWidget* map_widget_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;      // manual refresh
    QPushButton* auto_refresh_btn_ = nullptr; // AUTO-refresh toggle (off by default)
    fincept::ui::LoadingOverlay* map_loader_ = nullptr;
    QTableWidget* vessels_table_ = nullptr;
    QTableWidget* routes_table_ = nullptr;
    // The corridors table shares its slot with a designed state ("no corridors").
    QStackedWidget* routes_stack_ = nullptr;
    fincept::ui::EmptyState* routes_state_ = nullptr;
    bool restored_map_view_ = false;  // restore_state() brought a saved map view

    // Right panel - search
    QLineEdit* imo_edit_ = nullptr;
    QLabel* search_result_label_ = nullptr;
    fincept::ui::Card* search_result_card_ = nullptr; // SELECTED VESSEL card (title = vessel name)
    QLabel* sr_imo_ = nullptr;
    QLabel* sr_position_ = nullptr;
    fincept::ui::KpiStrip* sr_kpis_ = nullptr;   // speed / heading / draught / progress / fix age
    fincept::ui::RangeBar* sr_progress_ = nullptr; // route progress, from_port -> to_port
    QLabel* sr_from_ = nullptr;
    QLabel* sr_to_ = nullptr;

    // Voyage-history summary above the vessel table (shown in history mode only).
    QWidget* history_panel_ = nullptr;
    fincept::ui::KpiStrip* history_kpis_ = nullptr;
    fincept::ui::TimeSeriesChart* history_chart_ = nullptr;
    QLabel* history_empty_ = nullptr; // "No positions recorded for IMO …"

    // Right panel - place search (Nominatim geocoder). Primary area-input UX:
    // user types a place name, picks a suggestion, its bbox fills the spinners
    // and triggers a vessel area-search.
    QLineEdit* place_query_edit_ = nullptr;
    QTableWidget* place_table_ = nullptr;
    QLabel* place_status_ = nullptr;
    QVector<services::maritime::GeoPlace> place_results_;
    // The geocoder / port-search request each typeahead is currently waiting on. Replies
    // can arrive out of order (a slow answer for "rot" after one for "rotterdam"), so
    // anything that isn't the latest request's context is dropped.
    QString place_req_ctx_;
    QString ports_req_ctx_;
    // Live typeahead: debounced geocoder search feeds a completer popup.
    QCompleter* place_completer_ = nullptr;
    QStringListModel* place_completer_model_ = nullptr;
    QTimer* place_debounce_ = nullptr;

    // Right panel - area search (raw lat/long). Kept behind an "Advanced"
    // expander; place search / map-draw are the primary inputs now.
    QWidget* advanced_area_box_ = nullptr;
    QDoubleSpinBox* area_min_lat_ = nullptr;
    QDoubleSpinBox* area_max_lat_ = nullptr;
    QDoubleSpinBox* area_min_lng_ = nullptr;
    QDoubleSpinBox* area_max_lng_ = nullptr;

    // Right panel - ports search (Wikidata + Marine Regions + OSM Overpass)
    QLineEdit* ports_query_edit_ = nullptr;
    QTableWidget* ports_table_ = nullptr;
    QLabel* ports_status_ = nullptr;
    QVector<services::maritime::PortRecord> port_results_;
    QCompleter* ports_completer_ = nullptr;
    QStringListModel* ports_completer_model_ = nullptr;
    QTimer* ports_debounce_ = nullptr;
    // Voyage endpoint port resolution: maps the expected PortsCatalog context
    // ("name:<port>") to the pin label to plot when its results arrive, so
    // these lookups don't leak into the user-facing ports table.
    QHash<QString, QString> voyage_port_ctx_;

    // Route detail
    QWidget* route_detail_ = nullptr;
    QLabel* rd_name_ = nullptr;
    QLabel* rd_value_ = nullptr;  // under way N of M
    QLabel* rd_status_ = nullptr; // average speed / route progress
    QLabel* rd_vessels_ = nullptr;
    QPushButton* rd_clear_btn_ = nullptr; // SHOW ALL VESSELS (drops the corridor filter)

    // Status
    QLabel* status_label_ = nullptr;
    QLabel* vessel_count_label_ = nullptr;
    QLabel* source_value_ = nullptr;  // status-bar SOURCE value (driven by /marine/health)
    QLabel* records_value_ = nullptr; // status-bar RECORDS value (db.total_records)

    // Timer
    QTimer* refresh_timer_ = nullptr;
    bool first_show_ = true;
    /// Set by load_global_sample(); consumed by on_vessels_loaded so that
    /// the next batch is spatially downsampled to ~200 globally-spread pins.
    bool pending_global_sample_ = false;
    /// Set by on_shapes_changed(); consumed by on_vessels_loaded so the next
    /// batch is filtered to vessels lying inside the user's drawn shapes.
    bool filter_to_shapes_ = false;
    /// True while the in-flight load was started by the auto-refresh timer: the map
    /// camera is left alone so a background refresh doesn't undo the user's zoom.
    bool auto_refresh_pending_ = false;
    /// Set by do_refresh() (REFRESH button / AUTO timer); consumed by the next area
    /// search so it skips the service's cached copy. Opening the screen and
    /// repeating a search reuse a copy up to 10 min old for free (8 credits each).
    bool force_area_fetch_ = false;

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
    QLabel* basemap_cap_ = nullptr;       // "BASEMAP" caption in the map toolbar
    QComboBox* map_type_combo_ = nullptr; // basemap selector
    QPushButton* ports_layer_btn_ = nullptr;       // map layer: Natural Earth major ports
    QPushButton* chokepoints_layer_btn_ = nullptr; // map layer: straits / canals
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
    // Status bar captions + static value.
    QLabel* sb_source_cap_ = nullptr;
    QLabel* sb_records_cap_ = nullptr;
    QLabel* sb_refresh_cap_ = nullptr;
    QLabel* sb_refresh_val_ = nullptr;

    // Requests this screen issued. MaritimeService's signals are app-wide — the
    // dashboard tile's hub refreshes emit vessel_found / vessel_history_loaded /
    // error_occurred too — so only answers to these are acted on.
    QString pending_track_imo_;   // TRACK
    QString pending_history_imo_; // VOYAGE HISTORY
    int area_requests_pending_ = 0;

    // Voyage-history mode: the table lists past fixes of one vessel.
    bool history_mode_ = false;
    QVector<services::maritime::VesselData> history_rows_;

    // Data
    /// The loaded fleet the corridors were built from (before the render cap).
    QVector<services::maritime::VesselData> fleet_vessels_;
    /// Corridor ("from -> to") the table and map are narrowed to; empty = all.
    QString route_filter_;
    QVector<services::maritime::TradeRoute> routes_;
    /// Vessels currently rendered as map pins. Indexed by MapPin::id so that
    /// pin_clicked can resolve a click back to the full record for display
    /// in the right-panel search-result card.
    QVector<services::maritime::VesselData> rendered_vessels_;
};

} // namespace fincept::screens
