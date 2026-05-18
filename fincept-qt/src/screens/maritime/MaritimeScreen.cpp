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
    // Auto-refresh re-issues whichever mode produced the most recent set: a
    // bbox load if the user had typed one, otherwise the global sample. This
    // avoids a no-op refresh after a first-show global view (the bbox
    // spinners stay at zero in that case).
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        const bool valid_bbox = area_min_lat_ && area_max_lat_ && area_min_lng_ && area_max_lng_
                             && area_max_lat_->value() > area_min_lat_->value()
                             && area_max_lng_->value() > area_min_lng_->value();
        if (valid_bbox) on_load_vessels();
        else            load_global_sample();
    });

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_theme(); });

    LOG_INFO("Maritime", "Screen constructed");
}

void MaritimeScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
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
                .arg(ui::fonts::TINY)
                .arg(ui::fonts::DATA_FAMILY));

    if (classified_label_)
        classified_label_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2; letter-spacing:1px;")
                                             .arg(ui::colors::TEXT_TERTIARY())
                                             .arg(ui::fonts::DATA_FAMILY));

    if (credits_label_)
        credits_label_->setStyleSheet(
            QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                    "padding:3px 10px; background:%3; border:1px solid %4; border-radius:2px;")
                .arg(ui::colors::TEXT_TERTIARY())
                .arg(ui::fonts::DATA_FAMILY)
                .arg(ui::colors::BG_SURFACE())
                .arg(ui::colors::BORDER_MED()));

    if (vessel_count_label_)
        vessel_count_label_->setStyleSheet(
            QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                    "padding:3px 10px; background:%3; border:1px solid %4; border-radius:2px;")
                .arg(ui::colors::INFO())
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
        set_status("READY", ui::colors::POSITIVE);

    if (route_detail_)
        route_detail_->setStyleSheet(
            QString("background:%1; border:1px solid %2; border-left:3px solid %2; border-radius:2px;")
                .arg(C(ui::colors::BG_SURFACE), C(ui::colors::AMBER)));

    if (search_result_card_)
        search_result_card_->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:2px;")
                                               .arg(C(ui::colors::BG_SURFACE), C(ui::colors::INFO)));

    if (imo_edit_)
        imo_edit_->setStyleSheet(input_ss());
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
