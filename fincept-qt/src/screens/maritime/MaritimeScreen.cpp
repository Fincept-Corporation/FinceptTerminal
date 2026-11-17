// src/screens/maritime/MaritimeScreen.cpp
//
// Core lifecycle: ctor, show/hide events, connect_service, apply_theme,
// map-loading overlay helpers, set_status, save_state/restore_state.
// Style helpers live in MaritimeScreen_internal.h. UI / handlers split:
//   - MaritimeScreen_Layout.cpp   — build_* methods + populate_routes_table
//   - MaritimeScreen_Handlers.cpp — action + service-event handlers
#include "screens/maritime/MaritimeScreen.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/maritime/MaritimeScreen_internal.h"
#include "services/maritime/GeocodingService.h"
#include "services/maritime/MaritimeService.h"
#include "services/maritime/PortsCatalog.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/LoadingOverlay.h"
#include "ui/widgets/WorldMapWidget.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonObject>
#include <QLocale>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QTabWidget>

namespace fincept::screens {

using namespace fincept::services::maritime;
using namespace fincept::screens::maritime_internal;

MaritimeScreen::MaritimeScreen(QWidget* parent) : QWidget(parent) {
    // routes_ starts empty — populated dynamically from loaded vessels.
    build_ui();
    connect_service();

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(5 * 60 * 1000); // 5 min
    // Auto-refresh is opt-in (the AUTO toggle in the map toolbar), so it never
    // silently burns API credits. The timer and the manual button share
    // do_refresh().
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        do_refresh();
        auto_refresh_pending_ = true; // after do_refresh(), which clears it for manual loads
    });

    // The root sheet is built from theme tokens: the theme manager re-styles it
    // (and the tone-coloured status label with it) on a theme / font change.
    // Only the vessel-name item brushes, which no stylesheet reaches, follow here
    // — no setStyleSheet() and no status reset inside theme_changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { recolor_vessel_names(); });

    LOG_DEBUG("Maritime", "Screen constructed");
}

void MaritimeScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // Only resume auto-refresh if the user had it enabled (off by default).
    if (auto_refresh_btn_ && auto_refresh_btn_->isChecked())
        refresh_timer_->start();
    if (first_show_) {
        first_show_ = false;
        // An area search costs 8 credits: opening the screen never buys one (it
        // used to run the global sample on every first open). The vessel-list
        // pane opens on the view with a kept result — the saved bbox, else the
        // global view — and shows it with its "as of"; with none, its preview.
        open_initial_view();
        // Pull module status + DB record count for the status bar (free).
        MaritimeService::instance().check_health();
    }
    LOG_DEBUG("Maritime", "Screen shown");
}

void MaritimeScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_->stop();
    // A typeahead debounce still pending when the screen is left would fire a
    // geocoder / Wikidata request for a screen nobody is looking at.
    if (place_debounce_)
        place_debounce_->stop();
    if (ports_debounce_)
        ports_debounce_->stop();
}

void MaritimeScreen::connect_service() {
    // These signals are service-wide (hub refreshes for dashboard tiles emit them
    // too); the handlers act only on answers to this screen's own requests
    // (pending_track_imo_ / pending_history_imo_ / area_requests_pending_).
    auto& svc = MaritimeService::instance();
    connect(&svc, &MaritimeService::vessels_loaded, this, &MaritimeScreen::on_vessels_loaded);
    connect(&svc, &MaritimeService::vessel_found, this, &MaritimeScreen::on_vessel_found);
    connect(&svc, &MaritimeService::vessel_history_loaded, this, &MaritimeScreen::on_vessel_history);
    connect(&svc, &MaritimeService::health_loaded, this, &MaritimeScreen::on_health_loaded);
    connect(&svc, &MaritimeService::error_occurred, this, &MaritimeScreen::on_error);
    // The balance moves with every billed call app-wide (and with purchases).
    connect(&auth::AuthManager::instance(), &auth::AuthManager::credit_balance_changed, this,
            [this](double balance) { update_credits(balance); });

    auto& ports = PortsCatalog::instance();
    connect(&ports, &PortsCatalog::ports_found, this, &MaritimeScreen::on_ports_found);
    connect(&ports, &PortsCatalog::error_occurred, this, &MaritimeScreen::on_ports_error);

    auto& geo = GeocodingService::instance();
    connect(&geo, &GeocodingService::places_found, this, &MaritimeScreen::on_places_found);
    connect(&geo, &GeocodingService::error_occurred, this, &MaritimeScreen::on_places_error);
}

// ── Theme apply (construction only; theme changes: recolor_vessel_names) ─────
void MaritimeScreen::apply_theme() {
    // ONE stylesheet for the whole screen (see MaritimeScreen_internal.h); widgets
    // select their rules by objectName / "mtRole" / "tone".
    setStyleSheet(maritime_root_ss());
    if (status_label_)
        set_status(tr("READY"), "positive");
    recolor_vessel_names();
}

void MaritimeScreen::recolor_vessel_names() {
    // Vessel-name text colours are item brushes, which no stylesheet reaches.
    if (vessels_table_) {
        for (int r = 0; r < vessels_table_->rowCount(); ++r) {
            if (auto* it = vessels_table_->item(r, 0))
                it->setForeground(mt_name_color(it->data(kMtStaleRole).toBool()));
        }
    }
}

void MaritimeScreen::do_refresh() {
    // An explicit refresh fetches, whatever the cache holds.
    force_area_fetch_ = true;
    // The last load was driven by drawn shapes: re-run it through them so the in-shape
    // filter survives (a plain bbox reload dropped it and showed every vessel in the
    // shapes' bounding box).
    if (filter_to_shapes_ && map_widget_ && !map_widget_->shapes().isEmpty()) {
        on_shapes_changed();
        return;
    }
    // The vessel list's current view (global, an area, a history) re-runs as is.
    if (vessels_pane_ && views_.contains(vessels_pane_->view())) {
        vessels_pane_->run();
        return;
    }
    // Re-issue whichever mode produced the most recent set: a bbox load if the
    // user had typed/selected one, otherwise the global sample.
    const bool valid_bbox = area_min_lat_ && area_max_lat_ && area_min_lng_ && area_max_lng_ &&
                            area_max_lat_->value() > area_min_lat_->value() &&
                            area_max_lng_->value() > area_min_lng_->value();
    if (valid_bbox)
        on_load_vessels();
    else
        load_global_sample();
}

void MaritimeScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void MaritimeScreen::retranslateUi() {
    // Top bar
    if (brand_label_)
        brand_label_->setText(tr("FINCEPT MARITIME INTELLIGENCE"));
    // credits_label_ / vessel_count_label_ carry live counts and refresh on the
    // next data update; only their idle defaults are re-applied here.
    if (credits_label_ && credits_label_->text() == QStringLiteral("CREDITS: —"))
        credits_label_->setText(tr("CREDITS: —"));

    // Left panel
    if (global_btn_)
        global_btn_->setText(tr("GLOBAL VIEW (200 VESSELS)"));
    if (load_btn_)
        load_btn_->setText(tr("LOAD VESSELS IN AREA"));
    if (draw_title_)
        draw_title_->setText(tr("DRAW AREA ON MAP"));
    if (sq_btn_)
        sq_btn_->setText(tr("◻ SQUARE"));
    if (ci_btn_)
        ci_btn_->setText(tr("◯ CIRCLE"));
    if (clr_btn_)
        clr_btn_->setText(tr("✕ CLEAR"));
    if (intel_title_)
        intel_title_->setText(tr("FLEET INTELLIGENCE"));
    if (stat_vessels_cap_)
        stat_vessels_cap_->setText(tr("LOADED"));
    if (stat_displayed_cap_)
        stat_displayed_cap_->setText(tr("IN REGION"));
    if (stat_moving_cap_)
        stat_moving_cap_->setText(tr("MOVING"));
    if (stat_speed_cap_)
        stat_speed_cap_->setText(tr("AVG SPEED"));
    if (stat_routes_cap_)
        stat_routes_cap_->setText(tr("ROUTES"));
    if (stat_ports_cap_)
        stat_ports_cap_->setText(tr("DEST PORTS"));
    if (routes_title_)
        routes_title_->setText(tr("TRADE CORRIDORS"));
    if (routes_table_)
        routes_table_->setHorizontalHeaderLabels({tr("Route"), tr("Vessels")});

    // Center
    if (center_title_)
        center_title_->setText(tr("LIVE VESSEL MAP"));
    if (refresh_btn_)
        refresh_btn_->setText(tr("⟳ REFRESH"));
    if (auto_refresh_btn_)
        auto_refresh_btn_->setText(auto_refresh_btn_->isChecked() ? tr("AUTO ⟳ 5m") : tr("AUTO ⟳ OFF"));
    if (basemap_cap_)
        basemap_cap_->setText(tr("BASEMAP"));
    if (ports_layer_btn_)
        ports_layer_btn_->setText(tr("PORTS"));
    if (chokepoints_layer_btn_)
        chokepoints_layer_btn_->setText(tr("CHOKEPOINTS"));
    if (vessels_table_)
        set_history_columns(history_mode_); // headers depend on fleet / voyage-history mode

    // Right panel
    if (search_title_)
        search_title_->setText(tr("VESSEL SEARCH"));
    if (imo_cap_)
        imo_cap_->setText(tr("IMO NUMBER"));
    if (imo_edit_)
        imo_edit_->setPlaceholderText(tr("e.g. 9344745"));
    if (track_btn_)
        track_btn_->setText(tr("TRACK"));
    if (history_btn_)
        history_btn_->setText(tr("VOYAGE HISTORY"));
    if (place_title_)
        place_title_->setText(tr("PLACE SEARCH"));
    if (place_cap_)
        place_cap_->setText(tr("PLACE NAME"));
    if (place_query_edit_)
        place_query_edit_->setPlaceholderText(tr("e.g. Strait of Malacca"));
    if (place_btn_)
        place_btn_->setText(tr("SEARCH PLACES"));
    if (place_table_)
        place_table_->setHorizontalHeaderLabels({tr("Place"), tr("Type")});
    if (area_toggle_)
        area_toggle_->setText(area_toggle_->isChecked() ? tr("▾ ADVANCED: RAW BBOX") : tr("▸ ADVANCED: RAW BBOX"));
    if (coord_min_lat_cap_)
        coord_min_lat_cap_->setText(tr("MIN LATITUDE"));
    if (coord_max_lat_cap_)
        coord_max_lat_cap_->setText(tr("MAX LATITUDE"));
    if (coord_min_lng_cap_)
        coord_min_lng_cap_->setText(tr("MIN LONGITUDE"));
    if (coord_max_lng_cap_)
        coord_max_lng_cap_->setText(tr("MAX LONGITUDE"));
    if (area_btn_)
        area_btn_->setText(tr("SEARCH AREA"));
    if (ports_title_)
        ports_title_->setText(tr("PORTS"));
    if (ports_cap_)
        ports_cap_->setText(tr("PORT NAME"));
    if (ports_query_edit_)
        ports_query_edit_->setPlaceholderText(tr("e.g. Rotterdam"));
    if (ports_btn_)
        ports_btn_->setText(tr("SEARCH PORTS"));
    if (ports_in_view_btn_)
        ports_in_view_btn_->setText(tr("PORTS IN AREA"));
    if (ports_table_)
        ports_table_->setHorizontalHeaderLabels({tr("Name"), tr("Country"), tr("Source")});
    if (rd_title_)
        rd_title_->setText(tr("SELECTED ROUTE"));
    if (rd_clear_btn_)
        rd_clear_btn_->setText(tr("SHOW ALL VESSELS"));

    // Status bar
    if (sb_source_cap_)
        sb_source_cap_->setText(tr("SOURCE:"));
    if (sb_records_cap_)
        sb_records_cap_->setText(tr("RECORDS:"));
    if (sb_refresh_cap_)
        sb_refresh_cap_->setText(tr("UPDATED:"));
    // sb_refresh_val_ holds the live last-update clock — don't reset it here.
    // status_label_ reflects the last operation; re-apply the idle READY label.
    if (status_label_)
        set_status(tr("READY"), "positive");

    // Detail-card rows + table cells + search-result labels are filled from
    // live data and refresh on the next selection / fetch.
}

void MaritimeScreen::show_map_loading(const QString& msg) {
    if (!map_loader_)
        return;
    // The overlay sizes from parent->rect() each time it's shown — that's
    // already the behaviour of LoadingOverlay::show_loading(), but resize
    // might have happened since construction so refresh the geometry first.
    if (map_widget_)
        map_loader_->setGeometry(map_widget_->rect());
    map_loader_->show_loading(msg);
}

void MaritimeScreen::hide_map_loading() {
    if (map_loader_)
        map_loader_->hide_loading();
}

void MaritimeScreen::set_status(const QString& text, const char* tone) {
    if (!status_label_)
        return;
    status_label_->setText(text);
    status_label_->setToolTip(QString()); // drop any earlier error detail
    mt_set_tone(status_label_, tone);
}

QVariantMap MaritimeScreen::save_state() const {
    QVariantMap s;
    if (imo_edit_)
        s.insert("imo", imo_edit_->text());
    if (area_min_lat_)
        s.insert("min_lat", area_min_lat_->value());
    if (area_max_lat_)
        s.insert("max_lat", area_max_lat_->value());
    if (area_min_lng_)
        s.insert("min_lng", area_min_lng_->value());
    if (area_max_lng_)
        s.insert("max_lng", area_max_lng_->value());
    if (map_widget_)
        s.insert("map", map_widget_->view_state()); // camera, basemap, PORTS / CHOKEPOINTS
    return s;
}

void MaritimeScreen::restore_state(const QVariantMap& state) {
    if (imo_edit_)
        imo_edit_->setText(state.value("imo").toString());
    if (area_min_lat_ && state.contains("min_lat"))
        area_min_lat_->setValue(state.value("min_lat").toDouble());
    if (area_max_lat_ && state.contains("max_lat"))
        area_max_lat_->setValue(state.value("max_lat").toDouble());
    if (area_min_lng_ && state.contains("min_lng"))
        area_min_lng_->setValue(state.value("min_lng").toDouble());
    if (area_max_lng_ && state.contains("max_lng"))
        area_max_lng_->setValue(state.value("max_lng").toDouble());
    const QVariantMap map_state = state.value("map").toMap();
    restored_map_view_ = !map_state.isEmpty();
    if (map_widget_ && !map_state.isEmpty()) {
        map_widget_->restore_view_state(map_state);
        if (map_type_combo_) {
            const QSignalBlocker block(map_type_combo_);
            map_type_combo_->setCurrentIndex(map_widget_->current_basemap_index());
        }
        if (ports_layer_btn_) {
            const QSignalBlocker block(ports_layer_btn_);
            ports_layer_btn_->setChecked(map_widget_->reference_layer(ui::MapLayer::Ports));
        }
        if (chokepoints_layer_btn_) {
            const QSignalBlocker block(chokepoints_layer_btn_);
            chokepoints_layer_btn_->setChecked(map_widget_->reference_layer(ui::MapLayer::Chokepoints));
        }
    }
}

// ── ISectionLinked ────────────────────────────────────────────────────────────

bool MaritimeScreen::open_section(const QString& section, const QString& focus) {
    // No tabs: the right column's three searches. A link only brings the field into view and gives
    // it the keyboard — TRACK / VOYAGE HISTORY (credits) and the place / port look-ups run when the
    // user starts them, never from here.
    const QString key = section.trimmed().toLower();
    const QString imo = focus.trimmed();
    QLineEdit* field = nullptr;
    if (key == QLatin1String("vessel") || (key.isEmpty() && !imo.isEmpty()))
        field = imo_edit_;
    else if (key == QLatin1String("places"))
        field = place_query_edit_;
    else if (key == QLatin1String("ports"))
        field = ports_query_edit_;
    else if (key.isEmpty())
        return true;
    else
        return false;
    if (!field)
        return true;
    if (field == imo_edit_ && !imo.isEmpty())
        imo_edit_->setText(imo); // the number only; TRACK looks it up
    for (QWidget* p = field->parentWidget(); p; p = p->parentWidget())
        if (auto* scroll = qobject_cast<QScrollArea*>(p)) {
            scroll->ensureWidgetVisible(field);
            break;
        }
    field->setFocus(Qt::OtherFocusReason);
    field->selectAll();
    return true;
}

} // namespace fincept::screens
