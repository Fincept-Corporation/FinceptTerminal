// src/screens/maritime/MaritimeScreen.cpp
//
// Core lifecycle: ctor, show/hide events, connect_service, apply_theme,
// map-loading overlay helpers, set_status, save_state/restore_state.
// Style helpers live in MaritimeScreen_internal.h. UI / handlers split:
//   - MaritimeScreen_Layout.cpp   — build_* methods + populate_routes_table
//   - MaritimeScreen_Handlers.cpp — action + service-event handlers
#include "screens/maritime/MaritimeScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/maritime/GeocodingService.h"
#include "services/maritime/MaritimeService.h"
#include "services/maritime/PortsCatalog.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
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

#include "screens/maritime/MaritimeScreen_internal.h"

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
    connect(refresh_timer_, &QTimer::timeout, this, &MaritimeScreen::do_refresh);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_theme(); });

    LOG_INFO("Maritime", "Screen constructed");
}

void MaritimeScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // Only resume auto-refresh if the user had it enabled (off by default).
    if (auto_refresh_btn_ && auto_refresh_btn_->isChecked())
        refresh_timer_->start();
    if (first_show_) {
        first_show_ = false;
        // Default first-show action: globally-spread sample so the map looks
        // populated instead of empty. If the user previously typed a valid
        // bbox (restored from ScreenStateManager), honour that instead.
        bool used_saved_bbox = false;
        if (area_min_lat_ && area_max_lat_ && area_min_lng_ && area_max_lng_) {
            const bool valid = area_max_lat_->value() > area_min_lat_->value()
                            && area_max_lng_->value() > area_min_lng_->value();
            if (valid) {
                on_load_vessels();
                used_saved_bbox = true;
            }
        }
        if (!used_saved_bbox)
            load_global_sample();
        // Pull module status + DB record count for the status bar — replaces
        // the previously-static "AIS FEED" label with the live API response.
        MaritimeService::instance().check_health();
    }
    LOG_INFO("Maritime", "Screen shown");
}

void MaritimeScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_->stop();
}

void MaritimeScreen::connect_service() {
    auto& svc = MaritimeService::instance();
    connect(&svc, &MaritimeService::vessels_loaded, this, &MaritimeScreen::on_vessels_loaded);
    connect(&svc, &MaritimeService::vessel_found, this, &MaritimeScreen::on_vessel_found);
    connect(&svc, &MaritimeService::vessel_history_loaded, this, &MaritimeScreen::on_vessel_history);
    connect(&svc, &MaritimeService::health_loaded, this, &MaritimeScreen::on_health_loaded);
    connect(&svc, &MaritimeService::error_occurred, this, &MaritimeScreen::on_error);

    auto& ports = PortsCatalog::instance();
    connect(&ports, &PortsCatalog::ports_found, this, &MaritimeScreen::on_ports_found);
    connect(&ports, &PortsCatalog::error_occurred, this, &MaritimeScreen::on_ports_error);

    auto& geo = GeocodingService::instance();
    connect(&geo, &GeocodingService::places_found, this, &MaritimeScreen::on_places_found);
    connect(&geo, &GeocodingService::error_occurred, this, &MaritimeScreen::on_places_error);
}

// ── Theme apply (called on construction + theme_changed) ─────────────────────
void MaritimeScreen::apply_theme() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    if (top_bar_)
        top_bar_->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                                    .arg(C(ui::colors::BG_RAISED), C(ui::colors::BORDER_MED)));

    if (brand_label_)
        brand_label_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:2px;")
                .arg(ui::colors::AMBER())
                .arg(ui::fonts::DATA)
                .arg(ui::fonts::DATA_FAMILY));

    if (classified_label_)
        classified_label_->setStyleSheet(QString("color:%1; font-size:11px; font-family:%2; letter-spacing:1px;")
                                             .arg(ui::colors::TEXT_SECONDARY())
                                             .arg(ui::fonts::DATA_FAMILY));

    if (credits_label_)
        credits_label_->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:700; font-family:%2;"
                    "padding:4px 10px; background:%3; border:1px solid %4; border-radius:2px;")
                .arg(ui::colors::TEXT_SECONDARY())
                .arg(ui::fonts::DATA_FAMILY)
                .arg(ui::colors::BG_SURFACE())
                .arg(ui::colors::BORDER_MED()));

    if (vessel_count_label_)
        vessel_count_label_->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:700; font-family:%2;"
                    "padding:4px 10px; background:%3; border:1px solid %4; border-radius:2px;")
                .arg(ui::colors::AMBER())
                .arg(ui::fonts::DATA_FAMILY)
                .arg(ui::colors::BG_SURFACE())
                .arg(ui::colors::BORDER_MED()));

    if (left_panel_)
        left_panel_->setStyleSheet(QString("background:%1; border-right:1px solid %2;")
                                       .arg(C(ui::colors::BG_SURFACE), C(ui::colors::BORDER_DIM)));

    if (right_panel_)
        right_panel_->setStyleSheet(QString("background:%1; border-left:1px solid %2;")
                                        .arg(C(ui::colors::BG_SURFACE), C(ui::colors::BORDER_DIM)));

    if (routes_table_)
        routes_table_->setStyleSheet(table_ss());
    if (vessels_table_)
        vessels_table_->setStyleSheet(table_ss());

    if (status_bar_)
        status_bar_->setStyleSheet(QString("background:%1; border-top:1px solid %2;")
                                       .arg(C(ui::colors::BG_SURFACE), C(ui::colors::BORDER_DIM)));

    if (status_label_)
        set_status(tr("READY"), ui::colors::POSITIVE);

    // Both cards scope their border to the container objectName so the border
    // doesn't cascade onto every child label (which made each row look boxed).
    if (route_detail_)
        route_detail_->setStyleSheet(
            QString("#mtRouteCard { background:%1; border:1px solid %2; border-left:3px solid %2;"
                    " border-radius:2px; }")
                .arg(C(ui::colors::BG_SURFACE), C(ui::colors::AMBER)));

    if (search_result_card_)
        search_result_card_->setStyleSheet(
            QString("#mtSearchCard { background:%1; border:1px solid %2; border-left:3px solid %2;"
                    " border-radius:2px; }")
                .arg(C(ui::colors::BG_SURFACE), C(ui::colors::AMBER)));

    if (imo_edit_)
        imo_edit_->setStyleSheet(input_ss());

    if (basemap_cap_)
        basemap_cap_->setStyleSheet(tiny_label_ss());
    if (map_type_combo_)
        map_type_combo_->setStyleSheet(combo_ss());
}

void MaritimeScreen::do_refresh() {
    // Re-issue whichever mode produced the most recent set: a bbox load if the
    // user had typed/selected one, otherwise the global sample.
    const bool valid_bbox = area_min_lat_ && area_max_lat_ && area_min_lng_ && area_max_lng_
                         && area_max_lat_->value() > area_min_lat_->value()
                         && area_max_lng_->value() > area_min_lng_->value();
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
    if (brand_label_)      brand_label_->setText(tr("FINCEPT MARITIME INTELLIGENCE"));
    if (classified_label_) classified_label_->setText(tr("VESSEL TRACKING // FINCEPT MARINE API"));
    // credits_label_ / vessel_count_label_ carry live counts and refresh on the
    // next data update; only their idle defaults are re-applied here.
    if (credits_label_ && credits_label_->text() == QStringLiteral("CREDITS: —"))
        credits_label_->setText(tr("CREDITS: —"));

    // Left panel
    if (global_btn_)   global_btn_->setText(tr("GLOBAL VIEW (200 VESSELS)"));
    if (load_btn_)     load_btn_->setText(tr("LOAD VESSELS IN AREA"));
    if (draw_title_)   draw_title_->setText(tr("DRAW AREA ON MAP"));
    if (sq_btn_)       sq_btn_->setText(tr("◻ SQUARE"));
    if (ci_btn_)       ci_btn_->setText(tr("◯ CIRCLE"));
    if (clr_btn_)      clr_btn_->setText(tr("✕ CLEAR"));
    if (intel_title_)  intel_title_->setText(tr("FLEET INTELLIGENCE"));
    if (stat_vessels_cap_)   stat_vessels_cap_->setText(tr("LOADED"));
    if (stat_displayed_cap_) stat_displayed_cap_->setText(tr("IN REGION"));
    if (stat_moving_cap_)    stat_moving_cap_->setText(tr("MOVING"));
    if (stat_speed_cap_)     stat_speed_cap_->setText(tr("AVG SPEED"));
    if (stat_routes_cap_)    stat_routes_cap_->setText(tr("ROUTES"));
    if (stat_ports_cap_)     stat_ports_cap_->setText(tr("DEST PORTS"));
    if (routes_title_) routes_title_->setText(tr("TRADE CORRIDORS"));
    if (routes_table_) routes_table_->setHorizontalHeaderLabels({tr("Route"), tr("Vessels"), tr("Status")});

    // Center
    if (center_title_) center_title_->setText(tr("LIVE VESSEL MAP"));
    if (basemap_cap_)  basemap_cap_->setText(tr("BASEMAP"));
    if (vessels_table_)
        vessels_table_->setHorizontalHeaderLabels(
            {tr("Name"), tr("IMO"), tr("Lat"), tr("Lng"), tr("Speed (kn)"), tr("Angle"),
             tr("From"), tr("To"), tr("Progress")});

    // Right panel
    if (search_title_) search_title_->setText(tr("VESSEL SEARCH"));
    if (imo_cap_)      imo_cap_->setText(tr("IMO NUMBER"));
    if (imo_edit_)     imo_edit_->setPlaceholderText(tr("e.g. 9344745"));
    if (track_btn_)    track_btn_->setText(tr("TRACK"));
    if (history_btn_)  history_btn_->setText(tr("VOYAGE HISTORY"));
    if (place_title_)  place_title_->setText(tr("PLACE SEARCH"));
    if (place_cap_)    place_cap_->setText(tr("PLACE NAME"));
    if (place_query_edit_) place_query_edit_->setPlaceholderText(tr("e.g. Strait of Malacca"));
    if (place_btn_)    place_btn_->setText(tr("SEARCH PLACES"));
    if (place_table_)  place_table_->setHorizontalHeaderLabels({tr("Place"), tr("Type")});
    if (area_toggle_)
        area_toggle_->setText(area_toggle_->isChecked() ? tr("▾ ADVANCED: RAW BBOX")
                                                        : tr("▸ ADVANCED: RAW BBOX"));
    if (coord_min_lat_cap_) coord_min_lat_cap_->setText(tr("MIN LATITUDE"));
    if (coord_max_lat_cap_) coord_max_lat_cap_->setText(tr("MAX LATITUDE"));
    if (coord_min_lng_cap_) coord_min_lng_cap_->setText(tr("MIN LONGITUDE"));
    if (coord_max_lng_cap_) coord_max_lng_cap_->setText(tr("MAX LONGITUDE"));
    if (area_btn_)     area_btn_->setText(tr("SEARCH AREA"));
    if (ports_title_)  ports_title_->setText(tr("PORTS"));
    if (ports_cap_)    ports_cap_->setText(tr("PORT NAME"));
    if (ports_query_edit_) ports_query_edit_->setPlaceholderText(tr("e.g. Rotterdam"));
    if (ports_btn_)    ports_btn_->setText(tr("SEARCH PORTS"));
    if (ports_in_view_btn_) ports_in_view_btn_->setText(tr("PORTS IN AREA"));
    if (ports_table_)  ports_table_->setHorizontalHeaderLabels({tr("Name"), tr("Country"), tr("Source")});
    if (rd_title_)     rd_title_->setText(tr("SELECTED ROUTE"));
    if (classified_footer_) classified_footer_->setText(tr("CLASSIFIED — AUTHORIZED PERSONNEL ONLY"));

    // Status bar
    if (sb_source_cap_)  sb_source_cap_->setText(tr("SOURCE:"));
    if (sb_records_cap_) sb_records_cap_->setText(tr("RECORDS:"));
    if (sb_refresh_cap_) sb_refresh_cap_->setText(tr("UPDATED:"));
    // sb_refresh_val_ holds the live last-update clock — don't reset it here.
    // status_label_ reflects the last operation; re-apply the idle READY label.
    if (status_label_) set_status(tr("READY"), ui::colors::POSITIVE);

    // Detail-card rows + table cells + search-result labels are filled from
    // live data and refresh on the next selection / fetch.
}

void MaritimeScreen::show_map_loading(const QString& msg) {
    if (!map_loader_) return;
    // The overlay sizes from parent->rect() each time it's shown — that's
    // already the behaviour of LoadingOverlay::show_loading(), but resize
    // might have happened since construction so refresh the geometry first.
    if (map_widget_) map_loader_->setGeometry(map_widget_->rect());
    map_loader_->show_loading(msg);
}

void MaritimeScreen::hide_map_loading() {
    if (map_loader_) map_loader_->hide_loading();
}

void MaritimeScreen::set_status(const QString& text, const ui::ColorToken& color) {
    if (!status_label_)
        return;
    status_label_->setText(text);
    status_label_->setStyleSheet(
        QString("color:%1; font-size:8px; font-weight:700; font-family:%2;").arg(C(color) ).arg(ui::fonts::DATA_FAMILY));
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
}

} // namespace fincept::screens
