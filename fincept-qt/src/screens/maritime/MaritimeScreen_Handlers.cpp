// src/screens/maritime/MaritimeScreen_Handlers.cpp
//
// Action + service-event handlers: load_global_sample, on_load_vessels,
// on_search_vessel, on_vessels_loaded, on_vessel_found, on_error,
// on_ports_found, on_ports_error, on_health_loaded, on_route_selected,
// update_intelligence, update_credits, update_map, rebuild_routes_from_vessels,
// on_vessel_history.
//
// Part of the partial-class split of MaritimeScreen.cpp.

#include "screens/maritime/MaritimeScreen.h"
#include "screens/maritime/MaritimeScreen_internal.h"

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
#include <QTime>

namespace fincept::screens {

using namespace fincept::services::maritime;
using namespace fincept::screens::maritime_internal;

void MaritimeScreen::load_global_sample() {
    // World bbox — the API caps at ±90 lat / ±180 lng, but stays well-behaved
    // with these slightly inset values. Worldwide this matches ~45k vessels;
    // we ask the server for a bounded slice (`limit`) instead of transferring
    // them all, then the per-cell downsampler in on_vessels_loaded reduces
    // that to ~100 pins across a 10x10 lat/lng grid so the map fills out
    // instead of clumping in the busiest shipping lanes.
    AreaSearchParams params;
    params.min_lat = -89.0;
    params.max_lat =  89.0;
    params.min_lng = -179.0;
    params.max_lng =  179.0;
    // Request more than the 100 we ultimately show so the grid downsampler has
    // a geographically diverse pool to spread across — the API returns busy
    // lanes first, so a tight server cap would cluster. ~22x less than 45k.
    params.limit = 2000;
    set_status(tr("LOADING GLOBAL..."), ui::colors::AMBER);
    pending_global_sample_ = true;
    filter_to_shapes_ = false;  // global view is never shape-filtered
    show_map_loading(tr("LOADING GLOBAL VESSELS"));
    MaritimeService::instance().search_vessels_by_area(params);
}

void MaritimeScreen::on_load_vessels() {
    if (!area_min_lat_ || !area_max_lat_ || !area_min_lng_ || !area_max_lng_)
        return;
    run_area_search(area_min_lat_->value(), area_max_lat_->value(),
                    area_min_lng_->value(), area_max_lng_->value());
}

void MaritimeScreen::run_area_search(double min_lat, double max_lat, double min_lng, double max_lng,
                                     bool filter_to_shapes) {
    if (max_lat <= min_lat || max_lng <= min_lng) {
        set_status(tr("INVALID BBOX"), ui::colors::AMBER);
        return;
    }
    // Filter the next result to drawn shapes only when this search was driven
    // by the draw tool; place / bbox searches show the whole bbox.
    filter_to_shapes_ = filter_to_shapes;
    // Reflect the bbox in the advanced spinners so the user can see / tweak
    // what a place / draw selection resolved to.
    if (area_min_lat_) area_min_lat_->setValue(min_lat);
    if (area_max_lat_) area_max_lat_->setValue(max_lat);
    if (area_min_lng_) area_min_lng_->setValue(min_lng);
    if (area_max_lng_) area_max_lng_->setValue(max_lng);

    AreaSearchParams params;
    params.min_lat = min_lat;
    params.max_lat = max_lat;
    params.min_lng = min_lng;
    params.max_lng = max_lng;
    // Cap the working set like the global view does — a large place bbox
    // (e.g. an ocean) can still match thousands of vessels.
    params.limit = 2000;
    set_status(tr("SEARCHING..."), ui::colors::AMBER);
    pending_global_sample_ = false;
    show_map_loading(tr("SEARCHING AREA"));
    MaritimeService::instance().search_vessels_by_area(params);
}

void MaritimeScreen::on_places_found(QVector<services::maritime::GeoPlace> places, QString context) {
    Q_UNUSED(context);
    place_results_ = places;
    if (!place_table_) return;

    place_table_->setSortingEnabled(false);
    place_table_->setRowCount(places.size());
    for (int i = 0; i < places.size(); ++i) {
        const auto& p = places[i];
        auto* name = new QTableWidgetItem(p.display_name);
        name->setToolTip(QString("%1\nbbox: %2,%3 → %4,%5")
                             .arg(p.display_name)
                             .arg(p.min_lat, 0, 'f', 3).arg(p.min_lng, 0, 'f', 3)
                             .arg(p.max_lat, 0, 'f', 3).arg(p.max_lng, 0, 'f', 3));
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
    if (!map_widget_) return;
    const auto& shapes = map_widget_->shapes();
    if (shapes.isEmpty()) {
        // All shapes cleared — drop the shape filter. Leave the current
        // vessel set as-is (don't auto-reload); user can pick another action.
        filter_to_shapes_ = false;
        return;
    }
    // Union bbox over all drawn shapes (each shape carries its own bbox,
    // including circles which store their bounding box).
    double mn_lat =  90.0, mx_lat = -90.0, mn_lng =  180.0, mx_lng = -180.0;
    for (const auto& s : shapes) {
        mn_lat = std::min(mn_lat, s.min_lat);
        mx_lat = std::max(mx_lat, s.max_lat);
        mn_lng = std::min(mn_lng, s.min_lng);
        mx_lng = std::max(mx_lng, s.max_lng);
    }
    run_area_search(mn_lat, mx_lat, mn_lng, mx_lng, /*filter_to_shapes=*/true);
}

void MaritimeScreen::on_places_error(const QString& context, const QString& message) {
    Q_UNUSED(context);
    LOG_WARN("Maritime", QString("Place search failed: %1").arg(message));
    if (place_status_)
        place_status_->setText(tr("Search failed: %1").arg(message));
}

void MaritimeScreen::on_search_vessel() {
    auto imo = imo_edit_->text().trimmed();
    if (imo.isEmpty())
        return;
    search_result_card_->setVisible(false);
    search_result_label_->setVisible(false);
    set_status(tr("TRACKING..."), ui::colors::AMBER);
    show_map_loading(tr("TRACKING VESSEL"));
    MaritimeService::instance().get_vessel_position(imo);
}

void MaritimeScreen::on_vessels_loaded(VesselsPage page) {
    hide_map_loading();
    // Server-reported total for the searched area, captured before any
    // downsampling/shape-filter overwrites page.total_count. This drives the
    // "IN REGION" stat — kept distinct from the LOADED count so the two never
    // get conflated (the old UI showed this 5.2K figure as the vessel count).
    const int region_total = page.total_count;
    // Spread mode — globally-sampled fetch was triggered. Bin the returned
    // vessels into a 10x10 lat/lng grid and pick up to 2 per cell so the
    // map gets coverage instead of a dense regional cluster (the API tends
    // to return everything from busy shipping lanes first).
    if (pending_global_sample_) {
        pending_global_sample_ = false;
        constexpr int kBins = 10;
        constexpr int kPerCell = 2;
        constexpr int kTargetCount = 200;
        QHash<int, QVector<int>> by_cell;  // cell index → vessel indices
        for (int i = 0; i < page.vessels.size(); ++i) {
            const auto& v = page.vessels[i];
            if (v.latitude == 0.0 && v.longitude == 0.0) continue;
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
    static constexpr int kRenderLimit = 200;
    const int render_n = qMin(page.vessels.size(), kRenderLimit);

    vessels_table_->setSortingEnabled(false);
    vessels_table_->setRowCount(render_n);

    for (int i = 0; i < render_n; ++i) {
        const auto& v = page.vessels[i];

        auto* name_item = new QTableWidgetItem(v.name);
        name_item->setForeground(QBrush(QColor(ui::colors::TEXT_PRIMARY.get())));
        vessels_table_->setItem(i, 0, name_item);
        vessels_table_->setItem(i, 1, new QTableWidgetItem(v.imo));

        auto* lat = new QTableWidgetItem(QString::number(v.latitude, 'f', 4));
        lat->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 2, lat);

        auto* lng = new QTableWidgetItem(QString::number(v.longitude, 'f', 4));
        lng->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 3, lng);

        auto* spd = new QTableWidgetItem(QString::number(v.speed, 'f', 1));
        spd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 4, spd);

        auto* ang = new QTableWidgetItem(QString::number(v.angle, 'f', 0));
        ang->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 5, ang);

        vessels_table_->setItem(i, 6, new QTableWidgetItem(v.from_port));
        vessels_table_->setItem(i, 7, new QTableWidgetItem(v.to_port));

        auto* prog = new QTableWidgetItem(v.route_progress > 0 ? QString("%1%").arg(v.route_progress, 0, 'f', 0) : "-");
        prog->setTextAlignment(Qt::AlignCenter);
        vessels_table_->setItem(i, 8, prog);
    }

    vessels_table_->setSortingEnabled(true);
    vessels_table_->resizeColumnsToContents();

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
    // update_map populates rendered_vessels_ + the moving/speed/ports stats;
    // run it before update_intelligence so LOADED = actual pins on the map.
    update_map(page.vessels.mid(0, kRenderLimit));
    update_intelligence(region_total, rendered_vessels_.size());

    // Real "UPDATED:" clock in the status bar — every successful load stamps it.
    if (sb_refresh_val_)
        sb_refresh_val_->setText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));

    if (page.vessels.size() > kRenderLimit)
        set_status(tr("READY (showing %1 of %2)").arg(render_n).arg(page.vessels.size()), ui::colors::AMBER);
    else
        set_status(tr("READY"), ui::colors::POSITIVE);
}

void MaritimeScreen::on_vessel_found(VesselData vessel) {
    hide_map_loading();
    search_result_card_->setVisible(true);
    search_result_label_->setVisible(false);
    sr_name_->setText(vessel.name);
    sr_imo_->setText(tr("IMO: %1").arg(vessel.imo));
    sr_position_->setText(tr("Position: %1, %2").arg(vessel.latitude, 0, 'f', 4).arg(vessel.longitude, 0, 'f', 4));
    sr_speed_->setText(tr("Speed: %1 kn").arg(vessel.speed, 0, 'f', 1));
    sr_from_->setText(tr("From: %1").arg(vessel.from_port.isEmpty() ? QStringLiteral("—") : vessel.from_port));
    sr_to_->setText(tr("To: %1").arg(vessel.to_port.isEmpty() ? QStringLiteral("—") : vessel.to_port));

    // Focus the map on the tracked vessel: drop a single amber pin and zoom in
    // on its current position so TRACK actually takes the user there.
    if (map_widget_ && (vessel.latitude != 0.0 || vessel.longitude != 0.0)) {
        fincept::ui::MapPin pin;
        pin.latitude  = vessel.latitude;
        pin.longitude = vessel.longitude;
        pin.label     = QString("%1 (%2)").arg(vessel.name, vessel.imo);
        pin.color     = QColor(ui::colors::AMBER.get());
        pin.radius    = 8.0;
        pin.id        = -1;
        rendered_vessels_.clear();         // single-vessel focus; no fleet to click
        map_widget_->set_pins({pin});      // also clears any old voyage path
        map_widget_->fly_to(vessel.latitude, vessel.longitude);
    }
    set_status(tr("READY"), ui::colors::POSITIVE);
}

void MaritimeScreen::on_error(const QString& context, const QString& message) {
    hide_map_loading();
    pending_global_sample_ = false;
    if (context == "vessel_position") {
        search_result_card_->setVisible(false);
        search_result_label_->setText(tr("Error: %1").arg(message));
        search_result_label_->setVisible(true);
    }
    set_status(tr("ERROR"), ui::colors::NEGATIVE);
    LOG_ERROR("Maritime", QString("[%1] %2").arg(context, message));
}

void MaritimeScreen::on_ports_found(QVector<services::maritime::PortRecord> ports, QString context) {
    // Voyage origin/destination lookup → plot a labeled port pin instead of
    // populating the user-facing ports table.
    if (voyage_port_ctx_.contains(context)) {
        const QString label = voyage_port_ctx_.take(context);
        if (!ports.isEmpty() && map_widget_) {
            const auto& p = ports.first();  // best match for the searched name
            fincept::ui::MapPin pin;
            pin.latitude  = p.latitude;
            pin.longitude = p.longitude;
            pin.label     = p.country.isEmpty() ? label : QString("%1 — %2").arg(label, p.country);
            // White marker — distinct from the green vessels and the amber trail.
            pin.color     = QColor(ui::colors::TEXT_PRIMARY.get());
            pin.radius    = 9.0;
            pin.id        = -1;
            map_widget_->add_pin(pin);
        }
        return;
    }

    port_results_ = ports;
    if (!ports_table_) return;

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
    if (ports_status_)
        ports_status_->setText(tr("Lookup failed: %1").arg(message));
}

void MaritimeScreen::on_health_loaded(QJsonObject payload) {
    // /marine/health envelope: {success, message, data:{module, status,
    //   database:{status, total_records}, mode, endpoints:{...}}}
    const QJsonObject d = payload.value("data").toObject();
    const QString module = d.value("module").toString();
    const QString status = d.value("status").toString();
    const qint64 total   = d.value("database").toObject().value("total_records").toVariant().toLongLong();

    if (source_value_) {
        const QString label = module.isEmpty() ? tr("FINCEPT MARINE API") : module.toUpper();
        const QString color = status == QStringLiteral("healthy")
                                  ? ui::colors::POSITIVE()
                                  : ui::colors::AMBER();
        source_value_->setText(label);
        source_value_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; font-family:%2;")
                                         .arg(color)
                                         .arg(ui::fonts::DATA_FAMILY));
    }
    if (records_value_) {
        const QString formatted = total > 0
            ? QLocale(QLocale::English).toString(total)
            : QStringLiteral("—");
        records_value_->setText(formatted);
    }
}

void MaritimeScreen::on_route_selected(int row) {
    if (row < 0 || row >= routes_.size()) {
        route_detail_->setVisible(false);
        return;
    }
    const auto& r = routes_[row];
    route_detail_->setVisible(true);
    rd_name_->setText(r.name);
    rd_value_->setText(r.value.isEmpty() ? tr("Trade Value: n/a") : tr("Trade Value: %1").arg(r.value));
    rd_status_->setText(tr("Status: %1").arg(r.status.isEmpty() ? QStringLiteral("-") : r.status.toUpper()));
    rd_status_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
                                  .arg(route_status_color(r.status).name())
                                  .arg(ui::fonts::DATA_FAMILY));
    rd_vessels_->setText(tr("Active Vessels: %1").arg(r.vessels));
}

void MaritimeScreen::update_intelligence(int region_total, int loaded) {
    auto fmt = [](int n) -> QString {
        if (n >= 1'000'000) return QString::number(n / 1'000'000.0, 'f', 1) + "M";
        if (n >= 1'000)     return QString::number(n / 1'000.0,     'f', 1) + "K";
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

void MaritimeScreen::update_credits(int remaining) {
    if (!credits_label_ || remaining < 0)
        return;
    const QString c = remaining < 20  ? ui::colors::NEGATIVE()
                    : remaining < 100 ? ui::colors::AMBER()
                                      : ui::colors::POSITIVE();
    QColor cc(c);
    auto rgb = QString("%1,%2,%3").arg(cc.red()).arg(cc.green()).arg(cc.blue());
    credits_label_->setText(tr("CREDITS: %1").arg(remaining));
    credits_label_->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                                          "padding:3px 10px; background:rgba(%3,0.08); border:1px solid rgba(%3,0.3);"
                                          "border-radius:2px;")
                                      .arg(c)
                                      .arg(ui::fonts::DATA_FAMILY)
                                      .arg(rgb));
}

void MaritimeScreen::update_map(const QVector<VesselData>& vessels) {
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
        pins.append({v.latitude, v.longitude, QString("%1 (%2)").arg(v.name, v.imo),
                     ui::colors::POSITIVE, 5.5, id});
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
    if (stat_ports_)
        stat_ports_->setText(QString::number(port_seen.size()));
    if (stat_moving_)
        stat_moving_->setText(QString::number(moving));
    if (stat_speed_)
        stat_speed_->setText(speed_n > 0
                                 ? QString("%1 kn").arg(speed_sum / speed_n, 0, 'f', 1)
                                 : QStringLiteral("—"));
    map_widget_->set_pins(pins);
    map_widget_->fit_to_pins();
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
        if (r.name.isEmpty()) {
            r.name = key;
            r.status = QStringLiteral("active");
        }
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
    hide_map_loading();
    update_credits(page.remaining_credits);
    const auto& history = page.history;
    if (history.isEmpty()) {
        set_status(tr("NO HISTORY"), ui::colors::AMBER);
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

        double t = static_cast<double>(total - 1 - i) / std::max(total - 1, 1);
        int r_c = 255;
        int g_c = static_cast<int>(165 + t * 90);
        int b_c = static_cast<int>(t * 100);
        int alpha = static_cast<int>(100 + t * 155);
        QColor color(r_c, g_c, b_c, alpha);

        double radius = 3.0 + t * 4.0;
        QString label = tr("%1 — %2 → %3 [%4]").arg(vessel_name, h.from_port, h.to_port, h.last_updated.left(10));
        pins.append({h.latitude, h.longitude, label, color, radius});
    }

    const auto& current = history.first();
    if (current.latitude != 0.0 || current.longitude != 0.0) {
        pins.append({current.latitude, current.longitude, tr("%1 — CURRENT POSITION").arg(vessel_name),
                     ui::colors::POSITIVE, 8.0});
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

    map_widget_->set_pins(pins);           // clears any previous track
    map_widget_->set_track_path(track);    // draw the connecting line
    map_widget_->fit_to_pins();

    // Resolve + plot the voyage's origin / destination ports from the latest
    // leg's from/to names (async; routed by the name: context in on_ports_found
    // so these never leak into the user-facing ports table).
    voyage_port_ctx_.clear();
    const QString from_p = current.from_port.trimmed();
    const QString to_p   = current.to_port.trimmed();
    if (!from_p.isEmpty()) {
        voyage_port_ctx_.insert(QStringLiteral("name:") + from_p.toLower(), tr("ORIGIN: %1").arg(from_p));
        PortsCatalog::instance().search_by_name(from_p, 5);
    }
    if (!to_p.isEmpty() && to_p.compare(from_p, Qt::CaseInsensitive) != 0) {
        voyage_port_ctx_.insert(QStringLiteral("name:") + to_p.toLower(), tr("DEST: %1").arg(to_p));
        PortsCatalog::instance().search_by_name(to_p, 5);
    }

    vessels_table_->setSortingEnabled(false);
    vessels_table_->setRowCount(history.size());
    for (int i = 0; i < history.size(); ++i) {
        const auto& v = history[i];
        auto* name_item = new QTableWidgetItem(v.name);
        name_item->setForeground(QBrush(QColor(ui::colors::AMBER.get())));
        vessels_table_->setItem(i, 0, name_item);
        vessels_table_->setItem(i, 1, new QTableWidgetItem(v.imo));
        auto* lat = new QTableWidgetItem(QString::number(v.latitude, 'f', 4));
        lat->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 2, lat);
        auto* lng = new QTableWidgetItem(QString::number(v.longitude, 'f', 4));
        lng->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 3, lng);
        auto* spd = new QTableWidgetItem(QString::number(v.speed, 'f', 1));
        spd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 4, spd);
        auto* ang = new QTableWidgetItem(QString::number(v.angle, 'f', 0));
        ang->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vessels_table_->setItem(i, 5, ang);
        vessels_table_->setItem(i, 6, new QTableWidgetItem(v.from_port));
        vessels_table_->setItem(i, 7, new QTableWidgetItem(v.to_port));
        vessels_table_->setItem(i, 8, new QTableWidgetItem(v.last_updated.left(10)));
    }
    vessels_table_->setSortingEnabled(true);
    vessels_table_->resizeColumnsToContents();

    const int reported = page.total_records > 0 ? page.total_records : total;
    set_status(tr("HISTORY: %1 (%2 positions)").arg(vessel_name).arg(reported), ui::colors::AMBER);
    LOG_INFO("Maritime", QString("Vessel history [%1]: %2 / %3 positions").arg(vessel_name).arg(total).arg(reported));
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

} // namespace fincept::screens
