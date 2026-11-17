// src/screens/maritime/MaritimeScreen_Handlers.cpp
//
// Action + service-event handlers: load_global_sample, on_load_vessels,
// on_search_vessel, on_vessels_loaded, on_vessel_found, on_error,
// on_ports_found, on_ports_error, on_health_loaded, on_route_selected,
// update_intelligence, update_credits, update_map, rebuild_routes_from_vessels,
// on_vessel_history.
//
// Part of the partial-class split of MaritimeScreen.cpp.

#include "auth/AuthManager.h"
#include "services/billing/PlanAccess.h"
#include "auth/AuthTypes.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "network/http/HttpClient.h"
#include "screens/maritime/MaritimeScreen.h"
#include "screens/maritime/MaritimeScreen_internal.h"
#include "services/maritime/MaritimeService.h"
#include "services/maritime/PortsCatalog.h"
#include "ui/charts/RangeBar.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/Card.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingOverlay.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/WorldMapWidget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTime>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens {

using namespace fincept::services::maritime;
using namespace fincept::screens::maritime_internal;

namespace {

/// Parse the API's `last_updated` stamp and describe how stale it is. AIS
/// positions are frequently hours or days old; rendering a lat/lng with no age
/// invites reading a stale fix as a live one.
struct PositionAge {
    bool known = false;
    qint64 minutes = 0;
    QDateTime when;
};

PositionAge position_age(const QString& last_updated) {
    PositionAge a;
    if (last_updated.trimmed().isEmpty())
        return a;
    QDateTime dt = QDateTime::fromString(last_updated, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(last_updated, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid())
        return a;
    if (dt.timeSpec() == Qt::LocalTime)
        dt.setTimeZone(QTimeZone::UTC); // API stamps are UTC when unqualified
    a.known = true;
    a.when = dt;
    a.minutes = dt.secsTo(QDateTime::currentDateTimeUtc()) / 60;
    return a;
}

QString age_text(const PositionAge& a) {
    if (!a.known)
        return QCoreApplication::translate("MaritimeScreen", "position age unknown");
    if (a.minutes < 0)
        return QCoreApplication::translate("MaritimeScreen", "just now");
    if (a.minutes < 60)
        return QCoreApplication::translate("MaritimeScreen", "%n min old", "", static_cast<int>(a.minutes));
    if (a.minutes < 60 * 48)
        return QCoreApplication::translate("MaritimeScreen", "%n hour(s) old", "", static_cast<int>(a.minutes / 60));
    return QCoreApplication::translate("MaritimeScreen", "%n day(s) old", "", static_cast<int>(a.minutes / 1440));
}

/// Anything older than this is flagged so the user doesn't trust the fix.
constexpr qint64 kStaleMinutes = 6 * 60;
/// Below this balance the credit counter turns red and PLANS appears.
constexpr double kMaritimeLowCredits = 20.0;
/// Most vessels rendered as table rows from one load.
constexpr int kMtRenderLimit = 200;
/// Most vessels put on the map from one load (the map clusters them; the table
/// lists the newest kMtRenderLimit).
constexpr int kMtMapLimit = 2000;
/// Item-data role on a voyage-history row's Lat cell: index into history_rows_.
constexpr int kMtHistoryIndexRole = Qt::UserRole + 2;

/// Right-aligned numeric cell that sorts by value (QTableWidgetItem sorts text).
QTableWidgetItem* mt_num_item(const QString& text, double sort_value) {
    auto* it = new fincept::ui::NumericTableWidgetItem(text, sort_value);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

/// Most vessels one area search asks for (servers that ignore `limit` are
/// capped client-side by MaritimeService).
constexpr int kMtSearchLimit = 2000;

/// The global view's search: the whole world, slightly inset.
AreaSearchParams mt_global_params() {
    AreaSearchParams p;
    p.min_lat = -89.0;
    p.max_lat = 89.0;
    p.min_lng = -179.0;
    p.max_lng = 179.0;
    // More than the ~200 pins shown, so the grid downsampler has a
    // geographically diverse pool (the API returns busy lanes first).
    p.limit = kMtSearchLimit;
    return p;
}

AreaSearchParams mt_area_params(double min_lat, double max_lat, double min_lng, double max_lng) {
    AreaSearchParams p;
    p.min_lat = min_lat;
    p.max_lat = max_lat;
    p.min_lng = min_lng;
    p.max_lng = max_lng;
    // Cap the working set like the global view — a large place bbox (an ocean)
    // can still match thousands of vessels.
    p.limit = kMtSearchLimit;
    return p;
}

} // namespace

void MaritimeScreen::load_global_sample() {
    // World bbox — the API caps at ±90 lat / ±180 lng, but stays well-behaved
    // with these slightly inset values. Worldwide this matches ~45k vessels;
    // we ask for a bounded slice (`limit`: the most recently updated vessels —
    // servers that ignore it send everything and MaritimeService keeps the most
    // recent `limit` itself), then the per-cell downsampler in apply_vessels_page
    // reduces that to ~200 pins across a 10x10 lat/lng grid so the map fills
    // out instead of clumping in the busiest shipping lanes.
    select_area_view(mt_global_params(), /*global=*/true, /*shapes=*/false);
    vessels_pane_->run();
}

void MaritimeScreen::on_load_vessels() {
    if (!area_min_lat_ || !area_max_lat_ || !area_min_lng_ || !area_max_lng_)
        return;
    run_area_search(area_min_lat_->value(), area_max_lat_->value(), area_min_lng_->value(), area_max_lng_->value());
}

void MaritimeScreen::run_area_search(double min_lat, double max_lat, double min_lng, double max_lng,
                                     bool filter_to_shapes) {
    if (max_lat <= min_lat || max_lng <= min_lng) {
        set_status(tr("INVALID BBOX"), "amber");
        return;
    }
    // Reflect the bbox in the advanced spinners so the user can see / tweak
    // what a place / draw selection resolved to.
    if (area_min_lat_)
        area_min_lat_->setValue(min_lat);
    if (area_max_lat_)
        area_max_lat_->setValue(max_lat);
    if (area_min_lng_)
        area_min_lng_->setValue(min_lng);
    if (area_max_lng_)
        area_max_lng_->setValue(max_lng);
    // A drawn-shape load filters its result to the shapes; place / bbox
    // searches show the whole bbox.
    select_area_view(mt_area_params(min_lat, max_lat, min_lng, max_lng), /*global=*/false, filter_to_shapes);
    vessels_pane_->run();
}

void MaritimeScreen::open_initial_view() {
    auto& svc = MaritimeService::instance();
    const bool bbox_valid = area_min_lat_ && area_max_lat_ && area_min_lng_ && area_max_lng_ &&
                            area_max_lat_->value() > area_min_lat_->value() &&
                            area_max_lng_->value() > area_min_lng_->value();
    const AreaSearchParams bbox =
        bbox_valid ? mt_area_params(area_min_lat_->value(), area_max_lat_->value(), area_min_lng_->value(),
                                    area_max_lng_->value())
                   : AreaSearchParams{};
    // The view with a kept result wins (saved bbox first); the pane then shows
    // it with its "as of" — else the preview of the saved bbox / global view.
    if (bbox_valid && svc.area_cached_at(bbox, nullptr))
        select_area_view(bbox, /*global=*/false, /*shapes=*/false);
    else if (svc.area_cached_at(mt_global_params(), nullptr) || !bbox_valid)
        select_area_view(mt_global_params(), /*global=*/true, /*shapes=*/false);
    else
        select_area_view(bbox, /*global=*/false, /*shapes=*/false);
}

QString MaritimeScreen::select_area_view(const AreaSearchParams& params, bool global, bool shapes) {
    QString key;
    if (global)
        key = QStringLiteral("global");
    else
        key = QStringLiteral("%1/%2/%3/%4/%5")
                  .arg(shapes ? QStringLiteral("shapes") : QStringLiteral("bbox"))
                  .arg(params.min_lat, 0, 'f', 4)
                  .arg(params.max_lat, 0, 'f', 4)
                  .arg(params.min_lng, 0, 'f', 4)
                  .arg(params.max_lng, 0, 'f', 4);
    MtView v;
    v.params = params;
    v.global = global;
    v.shapes = shapes;
    views_.insert(key, v);
    configure_vessels_preview(v);
    vessels_pane_->set_cost_credits(8, QStringLiteral("maritime"));
    vessels_pane_->set_view(key);
    return key;
}

void MaritimeScreen::configure_vessels_preview(const MtView& v) {
    const auto& session = auth::AuthManager::instance().session();
    auto* es = vessels_pane_->preview();
    es->set_compact(true);
    es->set_glyph(QStringLiteral("◈"), ui::EmptyState::Tone::Warning);
    if (!v.imo.isEmpty()) {
        es->set_title(tr("Voyage history of IMO %1").arg(v.imo));
        es->set_message(tr("Every reported position of this vessel: the trail on the map, speed over ground and the "
                           "fixes in the table. Loaded on request."));
        es->set_details({});
        // The price on this plan (half the list price on Pro).
        const QString history_cost = auth::format_credits(
            fincept::billing::PlanAccess::instance().effective_cost(QStringLiteral("maritime"), 5.0));
        es->set_note(tr("%1 credits per history").arg(history_cost), ui::EmptyState::Tone::Warning);
        es->set_primary_action(tr("LOAD HISTORY (%1 CREDITS)").arg(history_cost));
        es->set_secondary_action(QString());
        return;
    }
    es->set_title(tr("Live vessel positions"));
    es->set_message(tr("AIS positions of the ships in an area: plotted on the map with speed, heading and route "
                       "progress, the busiest trade corridors and fleet statistics. Loaded on request — opening this "
                       "screen never spends credits."));
    QList<QPair<QString, QString>> details;
    if (v.global)
        details.append({tr("Area"), tr("Global view — 200 ships spread worldwide")});
    else
        details.append({tr("Area"), tr("%1° … %2° N, %3° … %4° E")
                                        .arg(v.params.min_lat, 0, 'f', 1)
                                        .arg(v.params.max_lat, 0, 'f', 1)
                                        .arg(v.params.min_lng, 0, 'f', 1)
                                        .arg(v.params.max_lng, 0, 'f', 1)});
    details.append({tr("Or"), tr("a key shipping lane on the right, or draw an area")});
    details.append({tr("Kept"), tr("Shown again on your next start, free")});
    es->set_details(details, tr("WHAT LOAD FETCHES"));
    // The price on this plan (half the list price on Pro).
    const QString search_cost = auth::format_credits(
        fincept::billing::PlanAccess::instance().effective_cost(QStringLiteral("maritime"), 8.0));
    es->set_note(session.authenticated ? tr("%1 credits per area search · balance %2")
                                             .arg(search_cost, auth::format_credits(session.user_info.credit_balance))
                                       : tr("%1 credits per area search").arg(search_cost),
                 ui::EmptyState::Tone::Warning);
    es->set_primary_action(v.global ? tr("LOAD GLOBAL VIEW (%1 CREDITS)").arg(search_cost)
                                    : tr("LOAD THIS AREA (%1 CREDITS)").arg(search_cost));
    es->set_secondary_action(tr("SHOW PORTS & CHOKEPOINTS (FREE)"));
}

void MaritimeScreen::start_vessels_request(quint64 id) {
    ++cached_load_gen_; // a kept copy still being parsed must not land over this run
    const QString view = vessels_pane_->view();
    const auto it = views_.constFind(view);
    if (it == views_.constEnd()) {
        vessels_pane_->fail(id, tr("Pick an area, a key shipping lane or the global view first."),
                            tr("Nothing to load"));
        return;
    }
    vessels_request_id_ = id;
    if (!it->imo.isEmpty()) {
        set_status(tr("LOADING HISTORY..."), "amber");
        show_map_loading(tr("LOADING HISTORY"));
        pending_history_imo_ = it->imo; // set first: a cached copy is answered synchronously
        MaritimeService::instance().get_vessel_history(it->imo, std::exchange(force_area_fetch_, false));
        return;
    }
    auto_refresh_pending_ = false; // the AUTO timer sets it again right after do_refresh()
    pending_global_sample_ = it->global;
    filter_to_shapes_ = it->shapes;
    set_status(it->global ? tr("LOADING GLOBAL...") : tr("SEARCHING..."), "amber");
    show_map_loading(it->global ? tr("LOADING GLOBAL VESSELS") : tr("SEARCHING AREA"));
    // Before the call: a copy younger than 10 min is answered synchronously.
    expected_area_key_ = MaritimeService::area_request_key(it->params);
    ++area_requests_pending_;
    MaritimeService::instance().search_vessels_by_area(it->params, std::exchange(force_area_fetch_, false));
}

bool MaritimeScreen::remembered_vessels(const QString& view, QJsonValue* payload, QDateTime* as_of) {
    // An area's last result is the service's kept copy (no second copy here):
    // only its fetch time is read now; render parses it off the GUI thread.
    const auto it = views_.constFind(view);
    if (it == views_.constEnd() || it->shapes || !it->imo.isEmpty())
        return false;
    qint64 at = 0;
    if (!MaritimeService::instance().area_cached_at(it->params, &at))
        return false;
    *payload = QJsonObject{{QStringLiteral("view"), view}};
    *as_of = QDateTime::fromMSecsSinceEpoch(at);
    return true;
}

void MaritimeScreen::render_vessels_payload(const QJsonValue& payload, bool from_cache) {
    Q_UNUSED(from_cache);
    const QString view = payload.toObject().value(QStringLiteral("view")).toString();
    if (view.isEmpty() || view == applied_view_key_)
        return; // its vessels are what the table and map already show
    const auto it = views_.constFind(view);
    if (it == views_.constEnd() || it->shapes || !it->imo.isEmpty())
        return;
    // The view's vessels are not on screen (opening the screen, or back to an
    // area loaded earlier): its kept copy, parsed off the GUI thread — free.
    const quint64 gen = ++cached_load_gen_;
    const bool global = it->global;
    const bool started = MaritimeService::instance().load_area_cached(
        it->params, this, [this, gen, view, global](VesselsPage page) {
            if (gen != cached_load_gen_ || vessels_pane_->view() != view)
                return; // a run started, or the user moved on, meanwhile
            hide_map_loading();
            filter_to_shapes_ = false; // area / global views are never shape-filtered
            apply_vessels_page(std::move(page), global);
            applied_view_key_ = view;
        });
    if (started)
        show_map_loading(tr("OPENING THE LAST RESULT"));
}

void MaritimeScreen::on_places_found(QVector<services::maritime::GeoPlace> places, QString context) {
    if (context != place_req_ctx_)
        return; // reply to a query the user has already typed past
    place_results_ = places;
    if (!place_table_)
        return;

    place_table_->setSortingEnabled(false);
    place_table_->setRowCount(places.size());
    for (int i = 0; i < places.size(); ++i) {
        const auto& p = places[i];
        auto* name = new QTableWidgetItem(p.display_name);
        name->setToolTip(QString("%1\nbbox: %2,%3 → %4,%5")
                             .arg(p.display_name)
                             .arg(p.min_lat, 0, 'f', 3)
                             .arg(p.min_lng, 0, 'f', 3)
                             .arg(p.max_lat, 0, 'f', 3)
                             .arg(p.max_lng, 0, 'f', 3));
        place_table_->setItem(i, 0, name);
        place_table_->setItem(i, 1, new QTableWidgetItem(p.type));
    }
    place_table_->setSortingEnabled(true);
    place_table_->resizeColumnsToContents();

    // Feed the typeahead completer and pop it open while the user is typing.
    if (place_completer_model_) {
        QStringList names;
        names.reserve(places.size());
        for (const auto& p : places)
            names << p.display_name;
        place_completer_model_->setStringList(names);
        if (!names.isEmpty() && place_query_edit_ && place_query_edit_->hasFocus())
            place_completer_->complete();
    }

    if (place_status_) {
        if (places.isEmpty())
            place_status_->setText(tr("No places found."));
        else
            place_status_->setText(tr("%n result(s) — click a row to load vessels.", "", places.size()));
    }
}

void MaritimeScreen::on_shapes_changed() {
    if (!map_widget_)
        return;
    const auto& shapes = map_widget_->shapes();
    if (shapes.isEmpty()) {
        // All shapes cleared — drop the shape filter. Leave the current
        // vessel set as-is (don't auto-reload); user can pick another action.
        filter_to_shapes_ = false;
        return;
    }
    // Union bbox over all drawn shapes (each shape carries its own bbox,
    // including circles which store their bounding box).
    double mn_lat = 90.0, mx_lat = -90.0, mn_lng = 180.0, mx_lng = -180.0;
    for (const auto& s : shapes) {
        mn_lat = std::min(mn_lat, s.min_lat);
        mx_lat = std::max(mx_lat, s.max_lat);
        mn_lng = std::min(mn_lng, s.min_lng);
        mx_lng = std::max(mx_lng, s.max_lng);
    }
    run_area_search(mn_lat, mx_lat, mn_lng, mx_lng, /*filter_to_shapes=*/true);
}

void MaritimeScreen::on_places_error(const QString& context, const QString& message) {
    LOG_WARN("Maritime", QString("Place search failed: %1").arg(message));
    if (context != place_req_ctx_)
        return;
    if (place_status_)
        place_status_->setText(tr("Search failed: %1").arg(message));
}

bool MaritimeScreen::read_imo(QString* imo_out) {
    QString raw = imo_edit_->text().trimmed();
    if (raw.startsWith(QLatin1String("IMO"), Qt::CaseInsensitive))
        raw = raw.mid(3).trimmed();
    if (raw.isEmpty())
        return false;
    static const QRegularExpression kSevenDigits(QStringLiteral("^\\d{7}$"));
    if (!kSevenDigits.match(raw).hasMatch()) {
        search_result_card_->setVisible(false);
        search_result_label_->setText(tr("IMO numbers are 7 digits (e.g. 9344745)."));
        search_result_label_->setVisible(true);
        set_status(tr("INVALID IMO"), "amber");
        return false;
    }
    *imo_out = raw;
    return true;
}

void MaritimeScreen::on_search_vessel() {
    QString imo;
    if (!read_imo(&imo))
        return;
    search_result_card_->setVisible(false);
    search_result_label_->setVisible(false);
    set_status(tr("TRACKING..."), "amber");
    show_map_loading(tr("TRACKING VESSEL"));
    pending_track_imo_ = imo; // before the call: a cached copy is answered synchronously
    MaritimeService::instance().get_vessel_position(imo);
}

void MaritimeScreen::on_vessels_loaded(VesselsPage page) {
    // Only the answer to this screen's latest area search: the signal is
    // service-wide, and an older search of another area may answer after it.
    if (expected_area_key_.isEmpty() || page.request_key != expected_area_key_)
        return;
    expected_area_key_.clear();
    area_requests_pending_ = 0;
    const bool global_sample = std::exchange(pending_global_sample_, false);
    const qint64 fetched_at = page.fetched_at_ms > 0 ? page.fetched_at_ms : QDateTime::currentMSecsSinceEpoch();
    const QString view = vessels_pane_->running_view();
    apply_vessels_page(std::move(page), global_sample);
    // The pane remembers the view (its result is the service's kept copy) with the
    // time the data was bought — a copy reused inside 10 min keeps its own time.
    if (const quint64 id = std::exchange(vessels_request_id_, 0); id != 0) {
        applied_view_key_ = view;
        vessels_pane_->deliver(id, QJsonObject{{QStringLiteral("view"), view}},
                               QDateTime::fromMSecsSinceEpoch(fetched_at));
    }
}

void MaritimeScreen::apply_vessels_page(VesselsPage page, bool global_sample) {
    hide_map_loading();
    // Leaving voyage-history mode: the table goes back to fleet columns and the
    // history summary (or its empty state) is hidden.
    history_mode_ = false;
    history_rows_.clear();
    set_history_columns(false);
    // Server-reported total for the searched area, captured before any
    // downsampling/shape-filter overwrites page.total_count. This drives the
    // "IN REGION" stat — kept distinct from the LOADED count so the two never
    // get conflated (the old UI showed this 5.2K figure as the vessel count).
    const int region_total = page.total_count;
    // Spread mode — globally-sampled fetch was triggered. Bin the returned
    // vessels into a 10x10 lat/lng grid and pick up to 2 per cell so the
    // map gets coverage instead of a dense regional cluster (the API tends
    // to return everything from busy shipping lanes first).
    if (global_sample) {
        constexpr int kBins = 10;
        constexpr int kPerCell = 2;
        constexpr int kTargetCount = 200;
        QHash<int, QVector<int>> by_cell; // cell index → vessel indices
        for (int i = 0; i < page.vessels.size(); ++i) {
            const auto& v = page.vessels[i];
            if (v.latitude == 0.0 && v.longitude == 0.0)
                continue;
            const int la = qBound(0, int((v.latitude + 90.0) / 180.0 * kBins), kBins - 1);
            const int ln = qBound(0, int((v.longitude + 180.0) / 360.0 * kBins), kBins - 1);
            by_cell[la * kBins + ln].append(i);
        }
        QVector<VesselData> picked;
        picked.reserve(kTargetCount);
        for (auto it = by_cell.cbegin(); it != by_cell.cend() && picked.size() < kTargetCount; ++it) {
            const auto& idxs = it.value();
            const int take = std::min<int>(kPerCell, idxs.size());
            for (int k = 0; k < take && picked.size() < kTargetCount; ++k)
                picked.append(page.vessels[idxs[k]]);
        }
        // Replace the page contents with the spread sample so the table /
        // map / stats all see the same downsampled set.
        page.vessels = picked;
        page.total_count = picked.size();
    }

    // Shape filter — when the user drew areas, the API was queried with the
    // union bbox; narrow the result to vessels actually inside one of the
    // drawn shapes (true circle / multi-shape selection). total_count is reset
    // to the in-shape count so the badge reflects what's shown.
    if (filter_to_shapes_ && map_widget_ && !map_widget_->shapes().isEmpty()) {
        const auto& shapes = map_widget_->shapes();
        QVector<VesselData> in_shape;
        in_shape.reserve(page.vessels.size());
        for (const auto& v : page.vessels) {
            for (const auto& s : shapes) {
                if (s.contains(v.latitude, v.longitude)) {
                    in_shape.append(v);
                    break;
                }
            }
        }
        page.vessels = in_shape;
        page.total_count = in_shape.size();
    }

    // Render limit — area-search can still return a large set for a tight
    // user bbox. Pin/render the top (newest-first) batch to keep the UI
    // responsive; the server's region total is shown separately (IN REGION).
    const int render_n = qMin(static_cast<int>(page.vessels.size()), kMtRenderLimit);
    fleet_vessels_ = page.vessels; // corridors + their filter work on the whole loaded set
    route_filter_.clear();
    fill_fleet_table(page.vessels, kMtRenderLimit);

    // Surface multi-vessel "not_found" warnings if any IMOs were missing.
    if (not_found_label_) {
        if (!page.not_found.isEmpty()) {
            not_found_label_->setText(tr("Not found in DB: %1").arg(page.not_found.join(", ")));
            not_found_label_->setVisible(true);
        } else {
            not_found_label_->setVisible(false);
        }
    }

    update_credits(page.remaining_credits);
    // Routes first so the ROUTES stat reflects this load (not the previous one).
    rebuild_routes_from_vessels(page.vessels);
    populate_routes_table();
    if (routes_stack_) {
        if (routes_.isEmpty()) {
            routes_state_->set_content(QStringLiteral("⇄"), tr("No corridors in this load"),
                                       tr("None of the loaded vessels reports both a departure and a destination "
                                          "port. Load a busier area to see its trade corridors."));
            routes_stack_->setCurrentWidget(routes_state_);
        } else {
            routes_stack_->setCurrentWidget(routes_table_);
        }
    }
    // update_map populates rendered_vessels_ + the moving/speed/ports stats;
    // run it before update_intelligence so LOADED = actual pins on the map.
    update_map(page.vessels.mid(0, kMtMapLimit), /*fit_camera=*/!auto_refresh_pending_);
    auto_refresh_pending_ = false;
    update_intelligence(region_total, rendered_vessels_.size());
    if (map_widget_)
        map_widget_->set_overlay_message(rendered_vessels_.isEmpty() ? tr("No vessel positions reported in this area")
                                                                     : QString());

    // "UPDATED:" = when the server produced this data. A cached copy (reopened
    // screen, repeated search — no credits spent) keeps its original time.
    if (sb_refresh_val_) {
        const QDateTime fetched = page.fetched_at_ms > 0 ? QDateTime::fromMSecsSinceEpoch(page.fetched_at_ms)
                                                         : QDateTime::currentDateTime();
        sb_refresh_val_->setText(fetched.toString(QStringLiteral("HH:mm:ss")));
        const qint64 age_min = fetched.secsTo(QDateTime::currentDateTime()) / 60;
        sb_refresh_val_->setToolTip(age_min >= 1 ? tr("Cached copy, %1 min old — ⟳ REFRESH fetches the latest "
                                                      "(uses credits)")
                                                       .arg(age_min)
                                                 : QString());
    }
    // A cache hit carries no balance; show the session's (it follows billed calls).
    if (page.remaining_credits < 0) {
        const auto& session = auth::AuthManager::instance().session();
        if (session.authenticated)
            update_credits(session.user_info.credit_balance);
    }

    // A copy older than the service's 10-minute window was shown on open without
    // a request: say how old it is and what refreshing costs.
    const qint64 copy_age_min =
        page.fetched_at_ms > 0 ? (QDateTime::currentMSecsSinceEpoch() - page.fetched_at_ms) / 60000 : 0;
    if (page.remaining_credits < 0 && copy_age_min >= 10) {
        const QDateTime at = QDateTime::fromMSecsSinceEpoch(page.fetched_at_ms);
        const bool today = at.date() == QDate::currentDate();
        set_status(tr("AS OF %1 · ⟳ REFRESH = 8 CR")
                       .arg(at.toString(today ? QStringLiteral("HH:mm") : QStringLiteral("dd MMM HH:mm")).toUpper()),
                   "muted");
        if (status_label_)
            status_label_->setToolTip(tr("Vessel positions of your last load (%1), shown without spending credits. "
                                         "⟳ REFRESH fetches the latest (8 credits).")
                                          .arg(QLocale().toString(at, QLocale::ShortFormat)));
    } else if (page.vessels.size() > kMtRenderLimit) {
        set_status(tr("READY (table: newest %1 of %2, map: %3)")
                       .arg(render_n)
                       .arg(page.vessels.size())
                       .arg(rendered_vessels_.size()),
                   "amber");
    } else {
        set_status(tr("READY"), "positive");
    }
}

void MaritimeScreen::on_vessel_found(VesselData vessel) {
    // Only the answer to this screen's own TRACK: the service also emits this for
    // hub refreshes of dashboard tiles, which must not repaint this map.
    if (pending_track_imo_.isEmpty() || (!vessel.imo.isEmpty() && vessel.imo != pending_track_imo_))
        return;
    pending_track_imo_.clear();
    hide_map_loading();
    show_vessel_card(vessel);

    // Focus the map on the tracked vessel: a larger amber pin, and zoom in on its
    // current position so TRACK actually takes the user there.
    if (map_widget_ && (vessel.latitude != 0.0 || vessel.longitude != 0.0)) {
        fincept::ui::MapPin pin;
        pin.latitude = vessel.latitude;
        pin.longitude = vessel.longitude;
        pin.label = QString("%1 (%2)").arg(vessel.name, vessel.imo);
        pin.color = mt_marker_color(&ui::ThemeTokens::accent);
        pin.radius = 8.0;
        pin.id = -1;
        if (!history_mode_ && !rendered_vessels_.isEmpty()) {
            // Keep the loaded fleet on the map and clickable (its table rows stay
            // valid); redraw it so an earlier TRACK highlight does not pile up.
            const QVector<VesselData> fleet = rendered_vessels_;
            update_map(fleet, /*fit_camera=*/false, /*update_stats=*/false);
            map_widget_->add_pin(pin);
        } else {
            // No fleet loaded, or the table shows a voyage history whose trail is
            // replaced here: single-vessel focus, and no rows left that point at
            // fixes no longer on the map.
            rendered_vessels_.clear();
            if (history_mode_) {
                history_mode_ = false;
                history_rows_.clear();
                set_history_columns(false);
                vessels_table_->setRowCount(0);
            }
            map_widget_->set_pins({pin}); // also clears any old voyage path
        }
        map_widget_->fly_to(vessel.latitude, vessel.longitude);
    }
    set_status(tr("READY"), "positive");
}

void MaritimeScreen::on_error(const QString& context, const QString& message) {
    // The status-bar health probe runs alongside the first vessel load. Its failure
    // used to cancel that load's global-sample flag, hide its loading overlay and
    // flip the whole screen to ERROR while vessels were still on the way.
    if (context == QLatin1String("health")) {
        LOG_WARN("Maritime", QString("Health check failed: %1").arg(message));
        if (source_value_)
            source_value_->setText(tr("UNAVAILABLE"));
        return;
    }
    // error_occurred is service-wide: a dashboard tile's hub refresh failing must
    // not flip this screen to ERROR. Act only on failures of this screen's own
    // requests.
    if (context == QLatin1String("vessel_position")) {
        if (pending_track_imo_.isEmpty())
            return;
        pending_track_imo_.clear();
    } else if (context == QLatin1String("vessel_history")) {
        if (pending_history_imo_.isEmpty())
            return;
        const QString imo = std::exchange(pending_history_imo_, QString());
        // 404 = the server has no positions for this IMO: an empty result, not a fault.
        if (HttpClient::status_from_error(message.toStdString()) == 404 && history_panel_ && history_empty_) {
            hide_map_loading();
            history_kpis_->hide();
            history_chart_->hide();
            history_empty_->setText(tr("No positions recorded for IMO %1.").arg(imo));
            history_empty_->show();
            history_panel_->show();
            set_status(tr("NO HISTORY"), "amber");
            if (const quint64 id = std::exchange(vessels_request_id_, 0); id != 0)
                vessels_pane_->fail(id, tr("The server has no positions recorded for IMO %1.").arg(imo),
                                    tr("No voyage history"));
            return;
        }
    } else if (context == QLatin1String("area_search")) {
        if (expected_area_key_.isEmpty())
            return; // not a search of this screen's
        expected_area_key_.clear();
        area_requests_pending_ = 0;
    }
    hide_map_loading();
    pending_global_sample_ = false;
    auto_refresh_pending_ = false;
    if (context == "vessel_position") {
        search_result_card_->setVisible(false);
        search_result_label_->setText(tr("Error: %1").arg(message));
        search_result_label_->setVisible(true);
    } else if (context == "vessel_history") {
        search_result_label_->setText(tr("History unavailable: %1").arg(message));
        search_result_label_->setVisible(true);
    }
    const int http_status = HttpClient::status_from_error(message.toStdString());
    // An area search that failed leaves the map without vessels: say why on it.
    QString note;
    if (http_status == 401)
        note = tr("Sign in to Fincept to load vessel positions");
    else if (http_status == 402)
        note = tr("Out of Fincept credits — a vessel area search costs 8 credits");
    else if (http_status == 0)
        note = tr("Vessel positions could not be loaded (network / timeout) — ⟳ REFRESH retries");
    else
        note = tr("Vessel search failed: %1").arg(HttpClient::message_from_error(message.toStdString()).left(80));
    if (context == QLatin1String("area_search") && map_widget_ && rendered_vessels_.isEmpty())
        map_widget_->set_overlay_message(note);
    // The vessel list's pane: the failure + RETRY in the table's place when it
    // has nothing to show, else the result stays and its status says so.
    if (context == QLatin1String("area_search") || context == QLatin1String("vessel_history")) {
        if (const quint64 id = std::exchange(vessels_request_id_, 0); id != 0)
            vessels_pane_->fail(id,
                                http_status == 402
                                    ? fincept::billing::PlanAccess::instance().with_upgrade_note(
                                          tr("Out of Fincept credits. Top up or change your plan (PLANS in the top "
                                             "bar), then retry — a failed request is not charged."),
                                          QStringLiteral("maritime"))
                                    : note + QLatin1Char('.'),
                                http_status == 402 ? tr("Out of Fincept credits") : QString());
    }
    if (http_status == 402) {
        // Out of credits — a retry cannot help; say so and offer the plans.
        set_status(tr("OUT OF CREDITS"), "negative");
        update_credits(0);
    } else {
        set_status(tr("ERROR"), "negative");
    }
    if (status_label_)
        status_label_->setToolTip(QStringLiteral("[%1] %2").arg(context, message));
    LOG_DEBUG("Maritime", QString("[%1] %2").arg(context, message));
}

void MaritimeScreen::on_ports_found(QVector<services::maritime::PortRecord> ports, QString context) {
    // Voyage origin/destination lookup → plot a labeled port pin instead of
    // populating the user-facing ports table.
    if (voyage_port_ctx_.contains(context)) {
        const QString label = voyage_port_ctx_.take(context);
        if (!ports.isEmpty() && map_widget_) {
            const auto& p = ports.first(); // best match for the searched name
            fincept::ui::MapPin pin;
            pin.latitude = p.latitude;
            pin.longitude = p.longitude;
            pin.label = p.country.isEmpty() ? label : QString("%1 — %2").arg(label, p.country);
            // White marker — distinct from the green vessels and the amber trail.
            pin.color = mt_marker_color(&ui::ThemeTokens::text_primary);
            pin.radius = 9.0;
            pin.id = -1;
            map_widget_->add_pin(pin);
        }
        return;
    }

    if (context != ports_req_ctx_)
        return; // reply to a query the user has already typed past
    port_results_ = ports;
    if (!ports_table_)
        return;

    ports_table_->setSortingEnabled(false);
    ports_table_->setRowCount(ports.size());
    for (int i = 0; i < ports.size(); ++i) {
        const auto& p = ports[i];
        // Name cell carries the LOCODE in its tooltip — that's a useful
        // detail without burning a column for fields most rows won't have.
        auto* name = new QTableWidgetItem(p.name);
        if (!p.locode.isEmpty())
            name->setToolTip(QString("%1\nUN/LOCODE: %2").arg(p.name, p.locode));
        ports_table_->setItem(i, 0, name);
        ports_table_->setItem(i, 1, new QTableWidgetItem(p.country));
        ports_table_->setItem(i, 2, new QTableWidgetItem(services::maritime::port_source_name(p.source)));
    }
    ports_table_->setSortingEnabled(true);
    ports_table_->resizeColumnsToContents();

    if (ports_completer_model_) {
        QStringList names;
        names.reserve(ports.size());
        for (const auto& p : ports)
            names << p.name;
        ports_completer_model_->setStringList(names);
        if (!names.isEmpty() && ports_query_edit_ && ports_query_edit_->hasFocus())
            ports_completer_->complete();
    }

    if (ports_status_) {
        if (ports.isEmpty())
            ports_status_->setText(tr("No ports found."));
        else
            ports_status_->setText(tr("%n port(s) — click a row to plot.", "", ports.size()));
    }
}

void MaritimeScreen::on_ports_error(const QString& context, const QString& message) {
    LOG_WARN("Maritime", QString("Ports [%1]: %2").arg(context, message));
    // Voyage origin/destination lookups run behind the scenes; their failures (and
    // replies for superseded queries) don't belong in the user's ports box.
    voyage_port_ctx_.remove(context);
    if (context != ports_req_ctx_)
        return;
    if (ports_status_)
        ports_status_->setText(tr("Lookup failed: %1").arg(message));
}

void MaritimeScreen::on_health_loaded(QJsonObject payload) {
    // /marine/health envelope: {success, message, data:{module, status,
    //   database:{status, total_records}, mode, endpoints:{...}}}
    const QJsonObject d = payload.value("data").toObject();
    const QString module = d.value("module").toString();
    // The module-level "status" was a constant "healthy" on older servers; the
    // database status is what says whether vessel data can be served.
    const QJsonObject db = d.value("database").toObject();
    const QString db_status = db.value("status").toString();
    const qint64 total = db.value("total_records").toVariant().toLongLong();
    const bool db_ok = db_status == QLatin1String("connected");

    if (source_value_) {
        QString label = module.isEmpty() ? tr("FINCEPT MARINE API") : module.toUpper();
        QString tip;
        if (db_status == QLatin1String("not_configured")) {
            label = tr("NOT CONFIGURED");
            tip = tr("The server has no marine database configured, so vessel searches cannot return data.");
        } else if (!db_ok) {
            label = tr("DB OFFLINE");
            tip = db_status.isEmpty()
                      ? tr("The server did not report the marine database status.")
                      : tr("The marine database is unreachable (%1); vessel searches will fail until it is back.")
                            .arg(db_status);
        }
        source_value_->setText(label);
        source_value_->setToolTip(tip);
        mt_set_tone(source_value_, db_ok ? "positive" : "amber");
    }
    // No vessels on the map and the server says it cannot serve any: say so there.
    if (!db_ok && map_widget_ && rendered_vessels_.isEmpty())
        map_widget_->set_overlay_message(tr("The marine database is offline on the server — vessel data unavailable"));
    if (records_value_) {
        const QString formatted = total > 0 ? QLocale().toString(total) : QStringLiteral("—");
        records_value_->setText(formatted);
        records_value_->setToolTip(db_ok ? QString() : tr("No record count while the marine database is offline."));
    }
}

void MaritimeScreen::on_route_selected(int row) {
    if (row < 0 || row >= routes_.size()) {
        // Selection cleared (a new load repopulated the table, or SHOW ALL).
        route_detail_->setVisible(false);
        route_filter_.clear();
        return;
    }
    const auto& r = routes_[row];
    // What the loaded set says about this corridor: its vessels, how many are
    // under way, their average speed and average route progress. (The API has
    // no corridor status or trade value, so none is shown.)
    QVector<VesselData> on_route;
    for (const auto& v : std::as_const(fleet_vessels_)) {
        if (!v.from_port.isEmpty() && !v.to_port.isEmpty() && v.from_port + " -> " + v.to_port == r.name)
            on_route.append(v);
    }
    int under_way = 0;
    double speed_sum = 0.0;
    double progress_sum = 0.0;
    int progress_n = 0;
    for (const auto& v : std::as_const(on_route)) {
        if (v.speed > 0.5) {
            ++under_way;
            speed_sum += v.speed;
        }
        if (v.route_progress > 0) {
            progress_sum += v.route_progress;
            ++progress_n;
        }
    }
    namespace fmt = fincept::ui::formatting;
    route_detail_->setVisible(true);
    rd_name_->setText(r.name);
    rd_vessels_->setText(tr("Vessels on corridor: %1").arg(QLocale().toString(on_route.size())));
    rd_value_->setText(tr("Under way (> 0.5 kn): %1 of %2").arg(under_way).arg(on_route.size()));
    mt_set_tone(rd_value_, under_way > 0 ? "positive" : "muted");
    rd_status_->setText(tr("Avg speed: %1  ·  Avg progress: %2")
                            .arg(under_way > 0 ? tr("%1 kn").arg(fmt::number(speed_sum / under_way, 1))
                                               : fmt::placeholder(),
                                 progress_n > 0 ? fmt::pct(progress_sum / progress_n, 0, false) : fmt::placeholder()));
    mt_set_tone(rd_status_, "muted");

    // Narrow the table and the map to this corridor's vessels.
    route_filter_ = r.name;
    if (!history_mode_ && !on_route.isEmpty()) {
        fill_fleet_table(on_route, kMtRenderLimit);
        update_map(on_route.mid(0, kMtMapLimit), /*fit_camera=*/true, /*update_stats=*/false);
        set_status(tr("CORRIDOR: %1 (%n vessel(s))", "", static_cast<int>(on_route.size())).arg(r.name), "amber");
    }
}

void MaritimeScreen::clear_route_filter() {
    if (routes_table_) {
        const QSignalBlocker block(routes_table_);
        routes_table_->clearSelection();
        routes_table_->setCurrentCell(-1, -1);
    }
    if (route_detail_)
        route_detail_->setVisible(false);
    const bool was_filtered = !route_filter_.isEmpty();
    route_filter_.clear();
    if (!was_filtered || history_mode_)
        return;
    fill_fleet_table(fleet_vessels_, kMtRenderLimit);
    update_map(fleet_vessels_.mid(0, kMtMapLimit), /*fit_camera=*/true, /*update_stats=*/false);
    set_status(tr("READY"), "positive");
}

void MaritimeScreen::update_intelligence(int region_total, int loaded) {
    auto fmt = [](int n) -> QString {
        if (n >= 1'000'000)
            return QString::number(n / 1'000'000.0, 'f', 1) + "M";
        if (n >= 1'000)
            return QString::number(n / 1'000.0, 'f', 1) + "K";
        return QString::number(n);
    };
    // Headline badge + LOADED stat = vessels actually on the map (truthful).
    vessel_count_label_->setText(tr("%1 VESSELS").arg(fmt(loaded)));
    if (stat_vessels_)
        stat_vessels_->setText(fmt(loaded));
    // IN REGION = server-reported total for the searched area (real, just bigger).
    if (stat_displayed_)
        stat_displayed_->setText(region_total > 0 ? fmt(region_total) : QStringLiteral("—"));
    if (stat_routes_)
        stat_routes_->setText(QString::number(routes_.size()));
}

void MaritimeScreen::update_credits(double remaining) {
    // < 0 = not reported (cache hits never report a balance — it would be stale).
    if (!credits_label_ || remaining < 0)
        return;
    credits_label_->setText(tr("CREDITS: %1").arg(auth::format_credits(remaining)));
    mt_set_tone(credits_label_, remaining < kMaritimeLowCredits ? "negative" : (remaining < 100.0 ? "amber" : "positive"));
    if (plans_btn_)
        plans_btn_->setVisible(remaining < kMaritimeLowCredits);
}

void MaritimeScreen::update_map(const QVector<VesselData>& vessels, bool fit_camera, bool update_stats) {
    rendered_vessels_.clear();
    rendered_vessels_.reserve(vessels.size());
    QVector<fincept::ui::MapPin> pins;
    pins.reserve(vessels.size());
    QSet<QString> port_seen;
    int moving = 0;
    double speed_sum = 0.0;
    int speed_n = 0;
    for (const auto& v : vessels) {
        if (v.latitude == 0.0 && v.longitude == 0.0)
            continue;
        // pin.id = index into rendered_vessels_ — pin_clicked uses it to
        // resolve the click back to the full record for the right-panel
        // detail card.
        const int id = rendered_vessels_.size();
        rendered_vessels_.append(v);
        // Pin label carries the fix age so hovering a pin tells the user how
        // old the position is; stale fixes are drawn amber instead of green.
        const PositionAge age = position_age(v.last_updated);
        const bool stale = !age.known || age.minutes >= kStaleMinutes;
        pins.append({v.latitude, v.longitude, QString("%1 (%2) — %3").arg(v.name, v.imo, age_text(age)),
                     mt_marker_color(stale ? &ui::ThemeTokens::accent : &ui::ThemeTokens::success), 5.5, id});
        // Track unique destination ports for the DEST PORTS stat.
        if (!v.to_port.isEmpty())
            port_seen.insert(v.to_port);
        // Fleet motion stats — under-way vessels report speed > ~0.5 kn.
        if (v.speed > 0.5) {
            ++moving;
            speed_sum += v.speed;
            ++speed_n;
        }
    }
    if (update_stats) {
        if (stat_ports_)
            stat_ports_->setText(QString::number(port_seen.size()));
        if (stat_moving_)
            stat_moving_->setText(QString::number(moving));
        if (stat_speed_)
            stat_speed_->setText(speed_n > 0 ? QString("%1 kn").arg(speed_sum / speed_n, 0, 'f', 1)
                                             : QStringLiteral("—"));
    }
    map_widget_->set_pins(pins);
    if (fit_camera)
        map_widget_->fit_to_pins();
}

void MaritimeScreen::show_vessel_card(const VesselData& v) {
    namespace fmt = fincept::ui::formatting;
    if (search_result_card_) {
        search_result_card_->set_title(v.name.isEmpty() ? tr("SELECTED VESSEL") : v.name.toUpper());
        search_result_card_->setVisible(true);
    }
    if (search_result_label_)
        search_result_label_->setVisible(false);
    if (sr_imo_)
        sr_imo_->setText(tr("IMO %1").arg(v.imo.isEmpty() ? fmt::placeholder() : v.imo));
    const PositionAge age = position_age(v.last_updated);
    const bool stale = !age.known || age.minutes >= kStaleMinutes;

    // Headline KPIs. Zero draught / progress means "not reported" in this feed.
    if (sr_kpis_) {
        using Tone = ui::StatTile::Tone;
        if (auto* t = sr_kpis_->tile(QStringLiteral("speed"))) {
            t->set_value(fmt::number(v.speed, 1));
            t->set_tone(v.speed > 0.5 ? Tone::Positive : Tone::Neutral);
            t->set_sub_label(v.speed > 0.5 ? tr("under way") : tr("stationary"));
        }
        if (auto* t = sr_kpis_->tile(QStringLiteral("heading")))
            t->set_value(QStringLiteral("%1°").arg(QLocale().toString(std::fmod(v.angle + 360.0, 360.0), 'f', 0)));
        if (auto* t = sr_kpis_->tile(QStringLiteral("draught")))
            t->set_value(v.draught > 0 ? fmt::number(v.draught, 1) : fmt::placeholder());
        if (auto* t = sr_kpis_->tile(QStringLiteral("progress")))
            t->set_value(v.route_progress > 0 ? fmt::pct(v.route_progress, 0, false) : fmt::placeholder());
        if (auto* t = sr_kpis_->tile(QStringLiteral("age"))) {
            t->set_value(age.known ? fmt::relative(age.when) : fmt::placeholder());
            t->set_tone(stale ? Tone::Warning : Tone::Positive);
            t->set_sub_label(stale ? tr("older than %n h", "", static_cast<int>(kStaleMinutes / 60)) : tr("fresh"));
            t->set_tooltip_text(age.known ? tr("Reported %1").arg(fmt::date_time(age.when))
                                          : tr("The API did not report a position timestamp for this vessel."));
        }
    }
    // Route progress between the two ports (the track runs 0 -> 100 %).
    if (sr_progress_) {
        const bool has_route = !v.from_port.isEmpty() || !v.to_port.isEmpty();
        sr_progress_->setVisible(has_route);
        if (has_route) {
            ui::RangeBarData d;
            d.low = 0.0;
            d.high = 100.0;
            if (v.route_progress > 0) {
                ui::RangeMarker m;
                m.value = std::clamp(v.route_progress, 0.0, 100.0);
                m.label = tr("progress");
                m.role = ui::RangeMarker::Role::Current;
                d.markers.append(m);
            }
            sr_progress_->set_data(d);
            sr_progress_->set_state(v.route_progress > 0 ? ui::PaintedChart::Ready : ui::PaintedChart::Empty,
                                    v.route_progress > 0 ? QString() : tr("No route progress reported"));
        }
    }
    auto port_line = [](const QString& caption, const QString& port, const QString& when) {
        QString s = caption + QStringLiteral(": ") + (port.isEmpty() ? fmt::placeholder() : port);
        if (!when.trimmed().isEmpty()) {
            const PositionAge t = position_age(when);
            s += QStringLiteral("  ·  ") + (t.known ? fmt::date(t.when) : when.left(10));
        }
        return s;
    };
    if (sr_from_)
        sr_from_->setText(port_line(tr("From"), v.from_port, v.from_date));
    if (sr_to_)
        sr_to_->setText(port_line(tr("To"), v.to_port, v.to_date));
    // Always state the age of the fix next to the coordinates.
    if (sr_position_) {
        sr_position_->setText(tr("Position: %1, %2  ·  %3")
                                  .arg(QLocale().toString(v.latitude, 'f', 4), QLocale().toString(v.longitude, 'f', 4),
                                       age_text(age)));
        sr_position_->setToolTip(age.known ? tr("Reported %1 UTC").arg(age.when.toUTC().toString(Qt::ISODate))
                                           : tr("The API did not report a position timestamp for this vessel."));
    }
    // Sync the IMO field so VOYAGE HISTORY works right after picking a vessel (never
    // blank the user's own entry when a record carries no IMO).
    if (imo_edit_ && !v.imo.isEmpty())
        imo_edit_->setText(v.imo);
}

void MaritimeScreen::on_vessel_row_picked(int row, bool fly) {
    if (!vessels_table_ || row < 0)
        return;
    if (history_mode_) {
        // A voyage-history row is one past fix: take the map there.
        auto* lat_item = vessels_table_->item(row, 2);
        if (!lat_item)
            return;
        const int idx = lat_item->data(kMtHistoryIndexRole).toInt();
        if (idx < 0 || idx >= history_rows_.size() || !map_widget_)
            return;
        const auto& h = history_rows_[idx];
        if (h.latitude != 0.0 || h.longitude != 0.0)
            map_widget_->fly_to(h.latitude, h.longitude, 7);
        return;
    }
    auto* imo_item = vessels_table_->item(row, 1); // IMO column
    if (!imo_item)
        return;
    for (int i = 0; i < rendered_vessels_.size(); ++i) {
        const auto& v = rendered_vessels_[i];
        if (v.imo != imo_item->text())
            continue;
        show_vessel_card(v);
        // The row's pin is highlighted (pin id = index into rendered_vessels_).
        if (map_widget_)
            map_widget_->set_selected_pin(i, /*reveal=*/!fly);
        if (fly && map_widget_ && (v.latitude != 0.0 || v.longitude != 0.0))
            map_widget_->fly_to(v.latitude, v.longitude);
        return;
    }
}

void MaritimeScreen::rebuild_routes_from_vessels(const QVector<VesselData>& vessels) {
    // Aggregate vessels by from_port -> to_port. Skip rows with missing
    // endpoints. Sort by vessel count desc so the busiest corridors top
    // the list.
    QHash<QString, TradeRoute> agg;
    for (const auto& v : vessels) {
        if (v.from_port.isEmpty() || v.to_port.isEmpty())
            continue;
        const QString key = v.from_port + " -> " + v.to_port;
        auto& r = agg[key];
        if (r.name.isEmpty())
            r.name = key; // no status: the data has none (see on_route_selected)
        r.vessels++;
    }
    routes_.clear();
    routes_.reserve(agg.size());
    for (auto it = agg.cbegin(); it != agg.cend(); ++it)
        routes_.append(it.value());
    std::sort(routes_.begin(), routes_.end(),
              [](const TradeRoute& a, const TradeRoute& b) { return a.vessels > b.vessels; });
}

void MaritimeScreen::on_vessel_history(VesselHistoryPage page) {
    // Only the answer to this screen's own VOYAGE HISTORY (hub refreshes for
    // dashboard tiles emit this signal too).
    if (pending_history_imo_.isEmpty() || (!page.imo.isEmpty() && page.imo != pending_history_imo_))
        return;
    const QString requested_imo = std::exchange(pending_history_imo_, QString());
    hide_map_loading();
    update_credits(page.remaining_credits);
    const auto& history = page.history;
    if (history.isEmpty()) {
        set_status(tr("NO HISTORY"), "amber");
        // Say it where the result would have been, not only in the status word.
        if (history_panel_ && history_empty_) {
            history_kpis_->hide();
            history_chart_->hide();
            history_empty_->setText(tr("No positions recorded for IMO %1.").arg(requested_imo));
            history_empty_->show();
            history_panel_->show();
        }
        if (const quint64 id = std::exchange(vessels_request_id_, 0); id != 0)
            vessels_pane_->fail(id, tr("The server has no positions recorded for IMO %1.").arg(requested_imo),
                                tr("No voyage history"));
        return;
    }

    QVector<fincept::ui::MapPin> pins;
    pins.reserve(history.size());

    QString vessel_name = history.first().name;
    int total = history.size();

    for (int i = 0; i < total; ++i) {
        const auto& h = history[i];
        if (h.latitude == 0.0 && h.longitude == 0.0)
            continue;

        // Oldest → newest fades from the accent to the warning yellow (marker
        // colours, see mt_marker_color), and from faint to opaque.
        double t = static_cast<double>(total - 1 - i) / std::max(total - 1, 1);
        const QColor from = mt_marker_color(&ui::ThemeTokens::accent);
        const QColor to = mt_marker_color(&ui::ThemeTokens::warning);
        QColor color(static_cast<int>(from.red() + (to.red() - from.red()) * t),
                     static_cast<int>(from.green() + (to.green() - from.green()) * t),
                     static_cast<int>(from.blue() + (to.blue() - from.blue()) * t));
        color.setAlpha(static_cast<int>(100 + t * 155));

        double radius = 3.0 + t * 4.0;
        QString label = tr("%1 — %2 → %3 [%4]").arg(vessel_name, h.from_port, h.to_port, h.last_updated.left(10));
        pins.append({h.latitude, h.longitude, label, color, radius});
    }

    const auto& current = history.first();
    if (current.latitude != 0.0 || current.longitude != 0.0) {
        pins.append({current.latitude, current.longitude, tr("%1 — CURRENT POSITION").arg(vessel_name),
                     mt_marker_color(&ui::ThemeTokens::success), 8.0});
    }

    // Connect the trail dots with a polyline so the voyage reads as a route,
    // not a scatter of points.
    QVector<QPointF> track;
    track.reserve(history.size());
    for (const auto& h : history) {
        if (h.latitude == 0.0 && h.longitude == 0.0)
            continue;
        track.append(QPointF(h.latitude, h.longitude));
    }

    rendered_vessels_.clear();          // the fleet pins are gone; history rows must not resolve to them
    map_widget_->set_pins(pins);        // clears any previous track
    map_widget_->set_track_path(track); // draw the connecting line
    map_widget_->fit_to_pins();

    // Resolve + plot the voyage's origin / destination ports from the latest
    // leg's from/to names (async; routed by the name: context in on_ports_found
    // so these never leak into the user-facing ports table).
    voyage_port_ctx_.clear();
    const QString from_p = current.from_port.trimmed();
    const QString to_p = current.to_port.trimmed();
    if (!from_p.isEmpty()) {
        voyage_port_ctx_.insert(QStringLiteral("name:") + from_p.toLower(), tr("ORIGIN: %1").arg(from_p));
        PortsCatalog::instance().search_by_name(from_p, 5);
    }
    if (!to_p.isEmpty() && to_p.compare(from_p, Qt::CaseInsensitive) != 0) {
        voyage_port_ctx_.insert(QStringLiteral("name:") + to_p.toLower(), tr("DEST: %1").arg(to_p));
        PortsCatalog::instance().search_by_name(to_p, 5);
    }

    namespace fmt = fincept::ui::formatting;
    // History mode: the table lists this vessel's past fixes. Name / IMO would
    // repeat on every row, so they are hidden (they are in the status line) and
    // the last column carries the report time instead of route progress.
    history_mode_ = true;
    history_rows_ = history;
    route_filter_.clear();
    if (route_detail_)
        route_detail_->setVisible(false);
    set_history_columns(true);

    vessels_table_->setSortingEnabled(false);
    vessels_table_->setRowCount(history.size());
    QVector<QPointF> speed_points;
    speed_points.reserve(history.size());
    double speed_sum = 0.0;
    double speed_max = 0.0;
    QDateTime first_fix, last_fix;
    for (int i = 0; i < history.size(); ++i) {
        const auto& v = history[i];
        const PositionAge age = position_age(v.last_updated);
        auto* name_item = new QTableWidgetItem(v.name);
        name_item->setData(kMtStaleRole, true);
        name_item->setForeground(mt_name_color(true));
        vessels_table_->setItem(i, 0, name_item);
        vessels_table_->setItem(i, 1, new QTableWidgetItem(v.imo));
        auto* lat = mt_num_item(QLocale().toString(v.latitude, 'f', 4), v.latitude);
        lat->setData(kMtHistoryIndexRole, i); // row -> history_rows_ (survives sorting)
        lat->setToolTip(tr("Click to show this fix on the map"));
        vessels_table_->setItem(i, 2, lat);
        vessels_table_->setItem(i, 3, mt_num_item(QLocale().toString(v.longitude, 'f', 4), v.longitude));
        vessels_table_->setItem(i, 4, mt_num_item(fmt::number(v.speed, 1), v.speed));
        vessels_table_->setItem(i, 5, mt_num_item(QLocale().toString(v.angle, 'f', 0), v.angle));
        vessels_table_->setItem(i, 6, new QTableWidgetItem(v.from_port));
        vessels_table_->setItem(i, 7, new QTableWidgetItem(v.to_port));
        // Date AND time: several fixes of one day used to read identically.
        vessels_table_->setItem(i, 8,
                                mt_num_item(age.known ? fmt::date_time(age.when, fmt::DateStyle::Short) : v.last_updated,
                                            age.known ? static_cast<double>(age.when.toMSecsSinceEpoch()) : 0.0));

        speed_sum += v.speed;
        speed_max = std::max(speed_max, v.speed);
        if (age.known) {
            speed_points.append(QPointF(static_cast<double>(age.when.toMSecsSinceEpoch()), v.speed));
            if (!first_fix.isValid() || age.when < first_fix)
                first_fix = age.when;
            if (!last_fix.isValid() || age.when > last_fix)
                last_fix = age.when;
        }
    }
    vessels_table_->setSortingEnabled(true);
    vessels_table_->resizeColumnsToContents();

    // Summary above the table: headline numbers + the speed profile over time.
    if (history_kpis_) {
        history_kpis_->show();
        if (auto* t = history_kpis_->tile(QStringLiteral("positions")))
            t->set_value(QLocale().toString(page.total_records > 0 ? page.total_records : total));
        if (auto* t = history_kpis_->tile(QStringLiteral("first")))
            t->set_value(first_fix.isValid() ? fmt::date_time(first_fix, fmt::DateStyle::Short) : fmt::placeholder());
        if (auto* t = history_kpis_->tile(QStringLiteral("last"))) {
            t->set_value(last_fix.isValid() ? fmt::date_time(last_fix, fmt::DateStyle::Short) : fmt::placeholder());
            t->set_sub_label(last_fix.isValid() ? fmt::relative(last_fix) : QString());
        }
        if (auto* t = history_kpis_->tile(QStringLiteral("avg")))
            t->set_value(fmt::number(speed_sum / std::max(1, total), 1));
        if (auto* t = history_kpis_->tile(QStringLiteral("max")))
            t->set_value(fmt::number(speed_max, 1));
    }
    if (history_chart_) {
        history_chart_->show();
        history_chart_->clear();
        if (speed_points.size() >= 2) {
            ui::TimeSeriesChart::SeriesSpec spec;
            spec.name = tr("Speed (kn)");
            spec.points = speed_points;
            spec.style = ui::TimeSeriesChart::Style::Area;
            spec.color = ui::TimeSeriesChart::ColorRole::Accent;
            spec.baseline = 0.0;
            spec.formatter = [](double v) { return fincept::ui::formatting::number(v, 1); };
            history_chart_->add_series(spec);
            history_chart_->set_state(ui::TimeSeriesChart::State::Ready);
        } else {
            history_chart_->set_state(ui::TimeSeriesChart::State::Empty,
                                      tr("Too few timestamped positions to chart the speed"));
        }
    }
    if (history_empty_)
        history_empty_->hide();
    if (history_panel_)
        history_panel_->show();

    const int reported = page.total_records > 0 ? page.total_records : total;
    set_status(tr("HISTORY: %1 · IMO %2 (%3 positions)")
                   .arg(vessel_name, page.imo.isEmpty() ? requested_imo : page.imo)
                   .arg(reported),
               "amber");
    LOG_DEBUG("Maritime", QString("Vessel history [%1]: %2 / %3 positions").arg(vessel_name).arg(total).arg(reported));
    // The vessel-list pane shows the fixes (the table) for this history view.
    if (const quint64 id = std::exchange(vessels_request_id_, 0); id != 0) {
        applied_view_key_ = vessels_pane_->running_view();
        vessels_pane_->deliver(id, QJsonObject{{QStringLiteral("view"), applied_view_key_}});
    }
}

void MaritimeScreen::set_history_columns(bool history) {
    if (!vessels_table_)
        return;
    vessels_table_->setHorizontalHeaderLabels({tr("Name"), tr("IMO"), tr("Lat"), tr("Lng"), tr("Speed (kn)"),
                                               tr("Angle"), tr("From"), tr("To"),
                                               history ? tr("Reported") : tr("Progress")});
    vessels_table_->setColumnHidden(0, history);
    vessels_table_->setColumnHidden(1, history);
    if (!history && history_panel_)
        history_panel_->hide();
}

void MaritimeScreen::fill_fleet_table(const QVector<VesselData>& vessels, int limit) {
    namespace fmt = fincept::ui::formatting;
    const int render_n = qMin(static_cast<int>(vessels.size()), limit);
    vessels_table_->setSortingEnabled(false);
    vessels_table_->setRowCount(render_n);
    for (int i = 0; i < render_n; ++i) {
        const auto& v = vessels[i];

        const PositionAge age = position_age(v.last_updated);
        const bool stale = !age.known || age.minutes >= kStaleMinutes;

        // Flag stale fixes by colouring the name amber (plus an age tooltip) so
        // a days-old AIS position can't be mistaken for a live one. Deliberately
        // no text prefix — that would corrupt sorting on the Name column.
        auto* name_item = new QTableWidgetItem(v.name);
        name_item->setData(kMtStaleRole, stale);
        name_item->setForeground(mt_name_color(stale));
        name_item->setToolTip(
            tr("%1\nIMO %2\nPosition reported: %3 (%4)")
                .arg(v.name, v.imo, age.known ? fmt::date_time(age.when) : tr("unknown"), age_text(age)));
        vessels_table_->setItem(i, 0, name_item);
        vessels_table_->setItem(i, 1, new QTableWidgetItem(v.imo));

        // Numeric columns sort by value (a text sort put 10.0 before 9.0).
        auto* lat = mt_num_item(QLocale().toString(v.latitude, 'f', 4), v.latitude);
        lat->setToolTip(age_text(age));
        vessels_table_->setItem(i, 2, lat);
        vessels_table_->setItem(i, 3, mt_num_item(QLocale().toString(v.longitude, 'f', 4), v.longitude));
        vessels_table_->setItem(i, 4, mt_num_item(fmt::number(v.speed, 1), v.speed));
        vessels_table_->setItem(i, 5, mt_num_item(QLocale().toString(v.angle, 'f', 0), v.angle));

        vessels_table_->setItem(i, 6, new QTableWidgetItem(v.from_port));
        vessels_table_->setItem(i, 7, new QTableWidgetItem(v.to_port));

        // "-" (not reported) sorts below every real progress value.
        auto* prog = mt_num_item(v.route_progress > 0 ? fmt::pct(v.route_progress, 0, false) : QStringLiteral("-"),
                                 v.route_progress > 0 ? v.route_progress : -1.0);
        vessels_table_->setItem(i, 8, prog);
    }
    vessels_table_->setSortingEnabled(true);
    vessels_table_->resizeColumnsToContents();
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

} // namespace fincept::screens
