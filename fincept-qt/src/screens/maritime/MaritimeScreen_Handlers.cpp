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

namespace fincept::screens {

using namespace fincept::services::maritime;
using namespace fincept::screens::maritime_internal;

void MaritimeScreen::load_global_sample() {
    // World bbox — the API caps at ±90 lat / ±180 lng, but stays well-behaved
    // with these slightly inset values. Returns ~45k vessels worldwide today;
    // the per-cell downsampler in on_vessels_loaded reduces that to ~200 pins
    // distributed across a 10x10 lat/lng grid so the map fills out instead of
    // clumping in the busiest shipping lanes.
    AreaSearchParams params;
    params.min_lat = -89.0;
    params.max_lat =  89.0;
    params.min_lng = -179.0;
    params.max_lng =  179.0;
    set_status("LOADING GLOBAL...", ui::colors::WARNING);
    pending_global_sample_ = true;
    show_map_loading(QStringLiteral("LOADING GLOBAL VESSELS"));
    MaritimeService::instance().search_vessels_by_area(params);
}

void MaritimeScreen::on_load_vessels() {
    AreaSearchParams params;
    if (area_min_lat_) params.min_lat = area_min_lat_->value();
    if (area_max_lat_) params.max_lat = area_max_lat_->value();
    if (area_min_lng_) params.min_lng = area_min_lng_->value();
    if (area_max_lng_) params.max_lng = area_max_lng_->value();
    if (params.max_lat <= params.min_lat || params.max_lng <= params.min_lng) {
        set_status("INVALID BBOX", ui::colors::WARNING);
        return;
    }
    set_status("LOADING...", ui::colors::WARNING);
    pending_global_sample_ = false;  // explicit bbox load — show as-is
    show_map_loading(QStringLiteral("LOADING VESSELS"));
    MaritimeService::instance().search_vessels_by_area(params);
}

void MaritimeScreen::on_search_vessel() {
    auto imo = imo_edit_->text().trimmed();
    if (imo.isEmpty())
        return;
    search_result_card_->setVisible(false);
    search_result_label_->setVisible(false);
    set_status("TRACKING...", ui::colors::WARNING);
    show_map_loading(QStringLiteral("TRACKING VESSEL"));
    MaritimeService::instance().get_vessel_position(imo);
}

void MaritimeScreen::on_vessels_loaded(VesselsPage page) {
    hide_map_loading();
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

    // Render limit — area-search can return 14k+ rows. Pin/render the top
    // (newest-first) batch to keep the UI responsive; full count still
    // shown in the status badge.
    static constexpr int kRenderLimit = 500;
    const int render_n = qMin(page.vessels.size(), kRenderLimit);

    vessels_table_->setSortingEnabled(false);
    vessels_table_->setRowCount(render_n);

    for (int i = 0; i < render_n; ++i) {
        const auto& v = page.vessels[i];

        auto* name_item = new QTableWidgetItem(v.name);
        name_item->setForeground(QBrush(QColor(ui::colors::INFO.get())));
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
            not_found_label_->setText(QString("Not found in DB: %1").arg(page.not_found.join(", ")));
            not_found_label_->setVisible(true);
        } else {
            not_found_label_->setVisible(false);
        }
    }

    update_intelligence(page.total_count);
    update_credits(page.remaining_credits);
    update_map(page.vessels.mid(0, kRenderLimit));
    rebuild_routes_from_vessels(page.vessels);
    populate_routes_table();

    if (page.vessels.size() > kRenderLimit)
        set_status(QString("READY (showing %1 of %2)").arg(render_n).arg(page.vessels.size()), ui::colors::WARNING);
    else
        set_status("READY", ui::colors::POSITIVE);
}

void MaritimeScreen::on_vessel_found(VesselData vessel) {
    hide_map_loading();
    search_result_card_->setVisible(true);
    search_result_label_->setVisible(false);
    sr_name_->setText(vessel.name);
    sr_imo_->setText("IMO: " + vessel.imo);
    sr_position_->setText(QString("Position: %1, %2").arg(vessel.latitude, 0, 'f', 4).arg(vessel.longitude, 0, 'f', 4));
    sr_speed_->setText(QString("Speed: %1 kn").arg(vessel.speed, 0, 'f', 1));
    sr_from_->setText("From: " + (vessel.from_port.isEmpty() ? "—" : vessel.from_port));
    sr_to_->setText("To: " + (vessel.to_port.isEmpty() ? "—" : vessel.to_port));
    set_status("READY", ui::colors::POSITIVE);
}

void MaritimeScreen::on_error(const QString& context, const QString& message) {
    hide_map_loading();
    pending_global_sample_ = false;
    if (context == "vessel_position") {
        search_result_card_->setVisible(false);
        search_result_label_->setText("Error: " + message);
        search_result_label_->setVisible(true);
    }
    set_status("ERROR", ui::colors::NEGATIVE);
    LOG_ERROR("Maritime", QString("[%1] %2").arg(context, message));
}

void MaritimeScreen::on_ports_found(QVector<services::maritime::PortRecord> ports, QString context) {
    Q_UNUSED(context);
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

    if (ports_status_) {
        if (ports.isEmpty())
            ports_status_->setText("No ports found.");
        else
            ports_status_->setText(QString("%1 port%2 — click a row to plot.")
                                       .arg(ports.size())
                                       .arg(ports.size() == 1 ? "" : "s"));
    }
}

void MaritimeScreen::on_ports_error(const QString& context, const QString& message) {
    LOG_WARN("Maritime", QString("Ports [%1]: %2").arg(context, message));
    if (ports_status_)
        ports_status_->setText("Lookup failed: " + message);
}

void MaritimeScreen::on_health_loaded(QJsonObject payload) {
    // /marine/health envelope: {success, message, data:{module, status,
    //   database:{status, total_records}, mode, endpoints:{...}}}
    const QJsonObject d = payload.value("data").toObject();
    const QString module = d.value("module").toString();
    const QString status = d.value("status").toString();
    const qint64 total   = d.value("database").toObject().value("total_records").toVariant().toLongLong();

    if (source_value_) {
        const QString label = module.isEmpty() ? QStringLiteral("FINCEPT MARINE API") : module.toUpper();
        const QString color = status == QStringLiteral("healthy")
                                  ? ui::colors::POSITIVE()
                                  : ui::colors::WARNING();
        source_value_->setText(label);
        source_value_->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
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
    rd_value_->setText(r.value.isEmpty() ? QStringLiteral("Trade Value: n/a") : "Trade Value: " + r.value);
    rd_status_->setText("Status: " + (r.status.isEmpty() ? QStringLiteral("-") : r.status.toUpper()));
    rd_status_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
                                  .arg(route_status_color(r.status).name())
                                  .arg(ui::fonts::DATA_FAMILY));
    rd_vessels_->setText(QString("Active Vessels: %1").arg(r.vessels));
}

void MaritimeScreen::update_intelligence(int vessel_count) {
    auto fmt = [](int n) -> QString {
        if (n >= 1'000'000) return QString::number(n / 1'000'000.0, 'f', 1) + "M";
        if (n >= 1'000)     return QString::number(n / 1'000.0,     'f', 1) + "K";
        return QString::number(n);
    };
    const QString count_str = fmt(vessel_count);
    vessel_count_label_->setText(QString("%1 VESSELS").arg(count_str));
    if (stat_vessels_)
        stat_vessels_->setText(count_str);
    if (stat_displayed_)
        stat_displayed_->setText(QString::number(qMin(vessel_count, 500)));
    if (stat_routes_)
        stat_routes_->setText(QString::number(routes_.size()));
}

void MaritimeScreen::update_credits(int remaining) {
    if (!credits_label_ || remaining < 0)
        return;
    const QString c = remaining < 20  ? ui::colors::NEGATIVE()
                    : remaining < 100 ? ui::colors::WARNING()
                                      : ui::colors::POSITIVE();
    QColor cc(c);
    auto rgb = QString("%1,%2,%3").arg(cc.red()).arg(cc.green()).arg(cc.blue());
    credits_label_->setText(QString("CREDITS: %1").arg(remaining));
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
    for (const auto& v : vessels) {
        if (v.latitude == 0.0 && v.longitude == 0.0)
            continue;
        // pin.id = index into rendered_vessels_ — pin_clicked uses it to
        // resolve the click back to the full record for the right-panel
        // detail card.
        const int id = rendered_vessels_.size();
        rendered_vessels_.append(v);
        pins.append({v.latitude, v.longitude, QString("%1 (%2)").arg(v.name, v.imo),
                     ui::colors::POSITIVE, 4.0, id});
        // Track unique destination ports for the PORTS stat.
        if (!v.to_port.isEmpty())
            port_seen.insert(v.to_port);
    }
    if (stat_ports_)
        stat_ports_->setText(QString::number(port_seen.size()));
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
        set_status("NO HISTORY", ui::colors::WARNING);
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
        QString label = QString("%1 — %2 → %3 [%4]").arg(vessel_name, h.from_port, h.to_port, h.last_updated.left(10));
        pins.append({h.latitude, h.longitude, label, color, radius});
    }

    const auto& current = history.first();
    if (current.latitude != 0.0 || current.longitude != 0.0) {
        pins.append({current.latitude, current.longitude, QString("%1 — CURRENT POSITION").arg(vessel_name),
                     ui::colors::POSITIVE, 8.0});
    }

    map_widget_->set_pins(pins);
    map_widget_->fit_to_pins();

    vessels_table_->setSortingEnabled(false);
    vessels_table_->setRowCount(history.size());
    for (int i = 0; i < history.size(); ++i) {
        const auto& v = history[i];
        auto* name_item = new QTableWidgetItem(v.name);
        name_item->setForeground(QBrush(QColor(ui::colors::WARNING.get())));
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
    set_status(QString("HISTORY: %1 (%2 positions)").arg(vessel_name).arg(reported), ui::colors::WARNING);
    LOG_INFO("Maritime", QString("Vessel history [%1]: %2 / %3 positions").arg(vessel_name).arg(total).arg(reported));
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

} // namespace fincept::screens
