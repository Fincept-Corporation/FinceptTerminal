// src/screens/maritime/MaritimeScreen_Layout.cpp
//
// UI construction: build_ui, build_top_bar, build_left_panel,
// build_center_panel, build_right_panel, build_status_bar, populate_routes_table.
//
// Part of the partial-class split of MaritimeScreen.cpp.

#include "screens/maritime/MaritimeScreen.h"
#include "screens/maritime/MaritimeScreen_internal.h"

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

namespace fincept::screens {

using namespace fincept::services::maritime;
using namespace fincept::screens::maritime_internal;

// ── Build UI ─────────────────────────────────────────────────────────────────
void MaritimeScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    top_bar_ = build_top_bar();
    root->addWidget(top_bar_);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    left_panel_ = build_left_panel();
    right_panel_ = build_right_panel();
    body->addWidget(left_panel_);
    body->addWidget(build_center_panel(), 1);
    body->addWidget(right_panel_);

    auto* body_w = new QWidget(this);
    body_w->setLayout(body);
    root->addWidget(body_w, 1);

    status_bar_ = build_status_bar();
    root->addWidget(status_bar_);

    apply_theme();
}

// ── Top Bar ──────────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_top_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(44);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(12);

    brand_label_ = new QLabel("FINCEPT MARITIME INTELLIGENCE", bar);
    hl->addWidget(brand_label_);

    classified_label_ = new QLabel("VESSEL TRACKING // FINCEPT MARINE API", bar);
    hl->addWidget(classified_label_);

    hl->addStretch(1);

    credits_label_ = new QLabel("CREDITS: —", bar);
    hl->addWidget(credits_label_);

    vessel_count_label_ = new QLabel("— VESSELS", bar);
    hl->addWidget(vessel_count_label_);

    return bar;
}

// ── Left Panel ───────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_left_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(260);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* global_btn = new QPushButton("GLOBAL VIEW (200 VESSELS)", panel);
    global_btn->setCursor(Qt::PointingHandCursor);
    global_btn->setStyleSheet(btn_primary_ss());
    connect(global_btn, &QPushButton::clicked, this, &MaritimeScreen::load_global_sample);
    vl->addWidget(global_btn);

    auto* load_btn = new QPushButton("LOAD VESSELS IN AREA", panel);
    load_btn->setCursor(Qt::PointingHandCursor);
    load_btn->setStyleSheet(btn_outline_ss());
    connect(load_btn, &QPushButton::clicked, this, &MaritimeScreen::on_load_vessels);
    vl->addWidget(load_btn);

    // ── Draw-on-map toolbar ─────────────────────────────────────────────────
    //
    // Square / Circle draw tools + Clear. The user can drop multiple shapes;
    // each finalized shape triggers on_shapes_changed which loads the union
    // bbox and filters returned vessels to those inside any drawn shape.
    auto* draw_title = new QLabel("DRAW AREA ON MAP", panel);
    draw_title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
                                  .arg(ui::colors::AMBER())
                                  .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(draw_title);

    auto* draw_row = new QHBoxLayout;
    draw_row->setSpacing(6);
    auto* sq_btn = new QPushButton("◻ SQUARE", panel);
    auto* ci_btn = new QPushButton("◯ CIRCLE", panel);
    auto* clr_btn = new QPushButton("✕ CLEAR", panel);
    for (auto* b : {sq_btn, ci_btn, clr_btn}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(btn_outline_ss());
    }
    sq_btn->setCheckable(true);
    ci_btn->setCheckable(true);
    draw_row->addWidget(sq_btn);
    draw_row->addWidget(ci_btn);
    draw_row->addWidget(clr_btn);
    vl->addLayout(draw_row);

    // Square / Circle are mutually exclusive toggles; toggling one off (or the
    // other on) returns the map to normal pan/zoom or switches tool.
    connect(sq_btn, &QPushButton::toggled, this, [this, ci_btn](bool on) {
        if (!map_widget_) return;
        if (on) ci_btn->setChecked(false);
        map_widget_->set_draw_mode(on ? fincept::ui::MapDrawMode::Rectangle
                                      : fincept::ui::MapDrawMode::None);
    });
    connect(ci_btn, &QPushButton::toggled, this, [this, sq_btn](bool on) {
        if (!map_widget_) return;
        if (on) sq_btn->setChecked(false);
        map_widget_->set_draw_mode(on ? fincept::ui::MapDrawMode::Circle
                                      : fincept::ui::MapDrawMode::None);
    });
    connect(clr_btn, &QPushButton::clicked, this, [this, sq_btn, ci_btn]() {
        sq_btn->setChecked(false);
        ci_btn->setChecked(false);
        if (map_widget_) {
            map_widget_->set_draw_mode(fincept::ui::MapDrawMode::None);
            map_widget_->clear_shapes();
        }
    });

    auto* intel_title = new QLabel("INTELLIGENCE", panel);
    intel_title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
                                   .arg(ui::colors::AMBER())
                                   .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(intel_title);

    auto* grid = new QGridLayout;
    grid->setSpacing(6);

    auto make_stat = [&](const QString& label, const QString& value, const ui::ColorToken& val_color) {
        auto* box = new QWidget(panel);
        box->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:2px;")
                               .arg(C(ui::colors::BG_BASE), C(ui::colors::BORDER_DIM)));
        auto* bvl = new QVBoxLayout(box);
        bvl->setContentsMargins(8, 6, 8, 6);
        bvl->setSpacing(2);
        auto* lbl = new QLabel(label, box);
        lbl->setStyleSheet(QString("color:%1; font-size:7px; font-family:%2;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::DATA_FAMILY));
        auto* val = new QLabel(value, box);
        val->setStyleSheet(QString("color:%1; font-size:12px; font-weight:700; font-family:%2;")
                               .arg(C(val_color))
                               .arg(ui::fonts::DATA_FAMILY));
        val->setObjectName(label);
        bvl->addWidget(lbl);
        bvl->addWidget(val);
        return box;
    };

    // All stats start at "—" and are populated from the loaded vessel set.
    // No hardcoded trade-volume, route count, or satellite numbers — the API
    // does not expose those, so we don't fabricate them.
    auto* sv = make_stat("TOTAL IN AREA", "—", ui::colors::INFO);
    stat_vessels_ = sv->findChild<QLabel*>("TOTAL IN AREA");
    grid->addWidget(sv, 0, 0);

    auto* sd = make_stat("DISPLAYED", "—", ui::colors::POSITIVE);
    stat_displayed_ = sd->findChild<QLabel*>("DISPLAYED");
    grid->addWidget(sd, 0, 1);

    auto* sr = make_stat("ROUTES", "—", ui::colors::INFO);
    stat_routes_ = sr->findChild<QLabel*>("ROUTES");
    grid->addWidget(sr, 1, 0);

    auto* sp = make_stat("PORTS", "—", ui::colors::WARNING);
    stat_ports_ = sp->findChild<QLabel*>("PORTS");
    grid->addWidget(sp, 1, 1);

    vl->addLayout(grid);

    vl->addSpacing(8);
    auto* routes_title = new QLabel("TRADE CORRIDORS", panel);
    routes_title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
                                    .arg(ui::colors::AMBER())
                                    .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(routes_title);

    routes_table_ = new QTableWidget(panel);
    routes_table_->setColumnCount(3);
    routes_table_->setHorizontalHeaderLabels({"Route", "Vessels", "Status"});
    routes_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    routes_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routes_table_->horizontalHeader()->setStretchLastSection(true);
    routes_table_->verticalHeader()->setVisible(false);
    routes_table_->setStyleSheet(table_ss());
    connect(routes_table_, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) { on_route_selected(row); });
    vl->addWidget(routes_table_, 1);

    return panel;
}

// ── Center Panel ─────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_center_panel() {
    auto* panel = new QWidget(this);
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header = new QWidget(panel);
    header->setFixedHeight(36);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(C(ui::colors::BG_RAISED), C(ui::colors::BORDER_DIM)));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    auto* title = new QLabel("VESSEL TRACKING", header);
    title->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                             .arg(ui::colors::AMBER())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);
    hhl->addStretch();
    vl->addWidget(header);

    auto* splitter = new QSplitter(Qt::Vertical, panel);
    splitter->setHandleWidth(2);
    splitter->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM()));

    map_widget_ = new fincept::ui::WorldMapWidget(splitter);
    map_widget_->setMinimumHeight(280);
    splitter->addWidget(map_widget_);
    // Overlay sits on top of the map and follows its size; it consumes mouse
    // events while visible so the user can't pan a half-loaded map.
    map_loader_ = new fincept::ui::LoadingOverlay(map_widget_);

    // Drawn-shape changes → load vessels for the union bbox and filter to
    // those inside the shapes.
    connect(map_widget_, &fincept::ui::WorldMapWidget::shapes_changed, this,
            &MaritimeScreen::on_shapes_changed);

    // Pin click → populate the right-panel SELECTED VESSEL card with the
    // record we cached in update_map. Clicks during a stale map (cached set
    // emptied) silently no-op.
    connect(map_widget_, &fincept::ui::WorldMapWidget::pin_clicked, this, [this](int id) {
        if (id < 0 || id >= rendered_vessels_.size()) return;
        const auto& v = rendered_vessels_[id];
        if (search_result_card_) search_result_card_->setVisible(true);
        if (search_result_label_) search_result_label_->setVisible(false);
        if (sr_name_)     sr_name_->setText(v.name);
        if (sr_imo_)      sr_imo_->setText("IMO: " + v.imo);
        if (sr_position_) sr_position_->setText(QString("Position: %1, %2")
                                                    .arg(v.latitude, 0, 'f', 4)
                                                    .arg(v.longitude, 0, 'f', 4));
        if (sr_speed_)    sr_speed_->setText(QString("Speed: %1 kn").arg(v.speed, 0, 'f', 1));
        if (sr_from_)     sr_from_->setText("From: " + (v.from_port.isEmpty() ? "—" : v.from_port));
        if (sr_to_)       sr_to_->setText("To: "   + (v.to_port.isEmpty()   ? "—" : v.to_port));
        // Sync the IMO field too so the user can hit "VOYAGE HISTORY" right
        // after clicking a pin without retyping.
        if (imo_edit_)    imo_edit_->setText(v.imo);
        // Highlight the corresponding row in the vessels table if it's
        // currently rendered (table may be in a different sort order).
        if (vessels_table_) {
            for (int r = 0; r < vessels_table_->rowCount(); ++r) {
                auto* item = vessels_table_->item(r, 1);  // IMO column
                if (item && item->text() == v.imo) {
                    vessels_table_->setCurrentCell(r, 0);
                    vessels_table_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
                    break;
                }
            }
        }
    });

    vessels_table_ = new QTableWidget(splitter);
    vessels_table_->setColumnCount(9);
    vessels_table_->setHorizontalHeaderLabels(
        {"Name", "IMO", "Lat", "Lng", "Speed (kn)", "Angle", "From", "To", "Progress"});
    vessels_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    vessels_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    vessels_table_->setAlternatingRowColors(true);
    vessels_table_->horizontalHeader()->setStretchLastSection(true);
    vessels_table_->verticalHeader()->setVisible(false);
    vessels_table_->setSortingEnabled(true);
    vessels_table_->setStyleSheet(table_ss());
    splitter->addWidget(vessels_table_);

    vessels_table_->setMinimumHeight(180);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 5);
    splitter->setSizes({380, 360});
    vl->addWidget(splitter, 1);

    return panel;
}

// ── Right Panel ───────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_right_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(280);

    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    auto* content = new QWidget(scroll);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(10);

    // ── Vessel Search ──────────────────────────────────────────────────────
    auto* search_title = new QLabel("VESSEL SEARCH", content);
    search_title->setStyleSheet(section_label_ss());
    vl->addWidget(search_title);

    auto* imo_lbl = new QLabel("IMO NUMBER", content);
    imo_lbl->setStyleSheet(tiny_label_ss());
    vl->addWidget(imo_lbl);

    imo_edit_ = new QLineEdit(content);
    imo_edit_->setPlaceholderText("e.g. 9344745");
    imo_edit_->setStyleSheet(input_ss());
    connect(imo_edit_, &QLineEdit::returnPressed, this, &MaritimeScreen::on_search_vessel);
    connect(imo_edit_, &QLineEdit::textChanged, this,
            [this](const QString&) { fincept::ScreenStateManager::instance().notify_changed(this); });
    vl->addWidget(imo_edit_);

    auto* track_btn = new QPushButton("TRACK", content);
    track_btn->setCursor(Qt::PointingHandCursor);
    track_btn->setStyleSheet(btn_primary_ss());
    connect(track_btn, &QPushButton::clicked, this, &MaritimeScreen::on_search_vessel);
    vl->addWidget(track_btn);

    auto* history_btn = new QPushButton("VOYAGE HISTORY", content);
    history_btn->setCursor(Qt::PointingHandCursor);
    history_btn->setStyleSheet(btn_outline_ss());
    connect(history_btn, &QPushButton::clicked, this, [this]() {
        auto imo = imo_edit_->text().trimmed();
        if (imo.isEmpty())
            return;
        set_status("LOADING HISTORY...", ui::colors::WARNING);
        show_map_loading(QStringLiteral("LOADING HISTORY"));
        MaritimeService::instance().get_vessel_history(imo);
    });
    vl->addWidget(history_btn);

    // Search result card
    search_result_card_ = new QWidget(content);
    search_result_card_->setVisible(false);
    auto* srvl = new QVBoxLayout(search_result_card_);
    srvl->setContentsMargins(10, 10, 10, 10);
    srvl->setSpacing(4);

    sr_name_ = new QLabel(search_result_card_);
    sr_name_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; font-family:%2;")
                                .arg(ui::colors::INFO())
                                .arg(ui::fonts::DATA_FAMILY));
    srvl->addWidget(sr_name_);
    sr_imo_ = new QLabel(search_result_card_);
    sr_imo_->setStyleSheet(detail_text_ss());
    srvl->addWidget(sr_imo_);
    sr_position_ = new QLabel(search_result_card_);
    sr_position_->setStyleSheet(detail_text_ss());
    srvl->addWidget(sr_position_);
    sr_speed_ = new QLabel(search_result_card_);
    sr_speed_->setStyleSheet(detail_text_ss());
    srvl->addWidget(sr_speed_);
    sr_from_ = new QLabel(search_result_card_);
    sr_from_->setStyleSheet(detail_text_ss());
    srvl->addWidget(sr_from_);
    sr_to_ = new QLabel(search_result_card_);
    sr_to_->setStyleSheet(detail_text_ss());
    srvl->addWidget(sr_to_);
    vl->addWidget(search_result_card_);

    search_result_label_ = new QLabel(content);
    search_result_label_->setStyleSheet(
        QString("color:%1; font-size:9px; font-family:%2;").arg(ui::colors::NEGATIVE()).arg(ui::fonts::DATA_FAMILY));
    search_result_label_->setVisible(false);
    vl->addWidget(search_result_label_);

    // ── Place Search ──────────────────────────────────────────────────────
    //
    // Primary area-input: type a place name, hit Search, pick a suggestion.
    // The selected result's bounding box fills the AREA SEARCH spinners and
    // immediately loads vessels in that area — no manual lat/long needed.
    // Geocoding via OpenStreetMap Nominatim (GeocodingService).
    vl->addSpacing(8);
    auto* place_title = new QLabel("PLACE SEARCH", content);
    place_title->setStyleSheet(section_label_ss());
    vl->addWidget(place_title);

    auto* place_lbl = new QLabel("PLACE NAME", content);
    place_lbl->setStyleSheet(tiny_label_ss());
    vl->addWidget(place_lbl);

    place_query_edit_ = new QLineEdit(content);
    place_query_edit_->setPlaceholderText("e.g. Strait of Malacca");
    place_query_edit_->setStyleSheet(input_ss());
    auto run_place_search = [this]() {
        const QString q = place_query_edit_->text().trimmed();
        if (q.isEmpty()) return;
        if (place_status_) place_status_->setText("Searching…");
        services::maritime::GeocodingService::instance().search(q);
    };
    connect(place_query_edit_, &QLineEdit::returnPressed, this, run_place_search);
    vl->addWidget(place_query_edit_);

    auto* place_btn = new QPushButton("SEARCH PLACES", content);
    place_btn->setCursor(Qt::PointingHandCursor);
    place_btn->setStyleSheet(btn_primary_ss());
    connect(place_btn, &QPushButton::clicked, this, run_place_search);
    vl->addWidget(place_btn);

    place_status_ = new QLabel(content);
    place_status_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
                                     .arg(ui::colors::TEXT_TERTIARY())
                                     .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(place_status_);

    place_table_ = new QTableWidget(content);
    place_table_->setColumnCount(2);
    place_table_->setHorizontalHeaderLabels({"Place", "Type"});
    place_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    place_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    place_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    place_table_->verticalHeader()->setVisible(false);
    place_table_->horizontalHeader()->setStretchLastSection(true);
    place_table_->setMinimumHeight(120);
    place_table_->setStyleSheet(table_ss());
    // Click a suggestion → fill bbox + load vessels in that area.
    connect(place_table_, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) {
        if (row < 0 || row >= place_results_.size()) return;
        const auto& p = place_results_[row];
        run_area_search(p.min_lat, p.max_lat, p.min_lng, p.max_lng);
    });
    vl->addWidget(place_table_);

    // ── Area Search (Advanced — raw lat/long) ───────────────────────────────
    vl->addSpacing(8);
    auto* area_toggle = new QPushButton("▸ ADVANCED: RAW BBOX", content);
    area_toggle->setCursor(Qt::PointingHandCursor);
    area_toggle->setCheckable(true);
    area_toggle->setStyleSheet(btn_outline_ss());
    vl->addWidget(area_toggle);

    advanced_area_box_ = new QWidget(content);
    advanced_area_box_->setVisible(false);
    auto* area_vl = new QVBoxLayout(advanced_area_box_);
    area_vl->setContentsMargins(0, 4, 0, 0);
    area_vl->setSpacing(8);

    auto make_coord = [&](const QString& label, double def_val) {
        auto* lbl = new QLabel(label, advanced_area_box_);
        lbl->setStyleSheet(tiny_label_ss());
        area_vl->addWidget(lbl);
        auto* spin = new QDoubleSpinBox(advanced_area_box_);
        spin->setRange(-180, 180);
        spin->setDecimals(4);
        spin->setValue(def_val);
        spin->setStyleSheet(input_ss());
        area_vl->addWidget(spin);
        return spin;
    };

    // Spinners start at zero — the user supplies a real bbox before the
    // first call (or restore_state injects the last-used coords).
    area_min_lat_ = make_coord("MIN LATITUDE", 0.0);
    area_max_lat_ = make_coord("MAX LATITUDE", 0.0);
    area_min_lng_ = make_coord("MIN LONGITUDE", 0.0);
    area_max_lng_ = make_coord("MAX LONGITUDE", 0.0);

    auto* area_btn = new QPushButton("SEARCH AREA", advanced_area_box_);
    area_btn->setCursor(Qt::PointingHandCursor);
    area_btn->setStyleSheet(btn_primary_ss());
    connect(area_btn, &QPushButton::clicked, this, [this]() {
        run_area_search(area_min_lat_->value(), area_max_lat_->value(),
                        area_min_lng_->value(), area_max_lng_->value());
    });
    area_vl->addWidget(area_btn);

    connect(area_toggle, &QPushButton::toggled, this, [this, area_toggle](bool on) {
        advanced_area_box_->setVisible(on);
        area_toggle->setText(on ? "▾ ADVANCED: RAW BBOX" : "▸ ADVANCED: RAW BBOX");
    });

    vl->addWidget(advanced_area_box_);

    // ── Ports Search ────────────────────────────────────────────────────────
    //
    // Free port directory (no API key, no static bundle): Wikidata SPARQL
    // primary, with Marine Regions and OSM Overpass as automatic fallbacks
    // inside PortsCatalog. Two entry points: free-text name search (works
    // anywhere in the world) and "ports in current bbox" (uses whatever
    // coords the user typed in the AREA SEARCH spinners above).
    vl->addSpacing(8);
    auto* ports_title = new QLabel("PORTS", content);
    ports_title->setStyleSheet(section_label_ss());
    vl->addWidget(ports_title);

    auto* ports_lbl = new QLabel("PORT NAME", content);
    ports_lbl->setStyleSheet(tiny_label_ss());
    vl->addWidget(ports_lbl);

    ports_query_edit_ = new QLineEdit(content);
    ports_query_edit_->setPlaceholderText("e.g. Rotterdam");
    ports_query_edit_->setStyleSheet(input_ss());
    auto run_name_search = [this]() {
        const QString q = ports_query_edit_->text().trimmed();
        if (q.isEmpty()) return;
        if (ports_status_) ports_status_->setText("Searching…");
        PortsCatalog::instance().search_by_name(q);
    };
    connect(ports_query_edit_, &QLineEdit::returnPressed, this, run_name_search);
    vl->addWidget(ports_query_edit_);

    auto* ports_btn = new QPushButton("SEARCH PORTS", content);
    ports_btn->setCursor(Qt::PointingHandCursor);
    ports_btn->setStyleSheet(btn_primary_ss());
    connect(ports_btn, &QPushButton::clicked, this, run_name_search);
    vl->addWidget(ports_btn);

    auto* ports_in_view_btn = new QPushButton("PORTS IN AREA", content);
    ports_in_view_btn->setCursor(Qt::PointingHandCursor);
    ports_in_view_btn->setStyleSheet(btn_outline_ss());
    connect(ports_in_view_btn, &QPushButton::clicked, this, [this]() {
        if (!area_min_lat_ || !area_max_lat_ || !area_min_lng_ || !area_max_lng_) return;
        const double mn_lat = area_min_lat_->value();
        const double mx_lat = area_max_lat_->value();
        const double mn_lng = area_min_lng_->value();
        const double mx_lng = area_max_lng_->value();
        if (mx_lat <= mn_lat || mx_lng <= mn_lng) {
            if (ports_status_) ports_status_->setText("Set a valid bbox in AREA SEARCH first.");
            return;
        }
        if (ports_status_) ports_status_->setText("Searching…");
        PortsCatalog::instance().search_by_bbox(mn_lat, mx_lat, mn_lng, mx_lng);
    });
    vl->addWidget(ports_in_view_btn);

    ports_status_ = new QLabel(content);
    ports_status_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
                                     .arg(ui::colors::TEXT_TERTIARY())
                                     .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(ports_status_);

    ports_table_ = new QTableWidget(content);
    ports_table_->setColumnCount(3);
    ports_table_->setHorizontalHeaderLabels({"Name", "Country", "Source"});
    ports_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ports_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    ports_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    ports_table_->verticalHeader()->setVisible(false);
    ports_table_->horizontalHeader()->setStretchLastSection(true);
    ports_table_->setMinimumHeight(160);
    ports_table_->setStyleSheet(table_ss());
    // Click → drop a CYAN pin on the existing world map to distinguish from
    // the green vessel pins, then re-fit the camera.
    connect(ports_table_, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) {
        if (row < 0 || row >= port_results_.size() || !map_widget_) return;
        const auto& p = port_results_[row];
        // Add port pin on top of existing vessel pins; id=-1 keeps the port
        // pin out of the pin_clicked dispatch (which expects vessel indices).
        fincept::ui::MapPin pin;
        pin.latitude  = p.latitude;
        pin.longitude = p.longitude;
        pin.label     = QString("%1 (%2)").arg(p.name, p.locode.isEmpty() ? p.country : p.locode);
        pin.color     = QColor(ui::colors::CYAN.get());
        pin.radius    = 6.0;
        pin.id        = -1;
        map_widget_->add_pin(pin);
        map_widget_->fly_to(p.latitude, p.longitude);
    });
    vl->addWidget(ports_table_);

    // ── Route Detail ───────────────────────────────────────────────────────
    vl->addSpacing(8);
    route_detail_ = new QWidget(content);
    route_detail_->setVisible(false);
    auto* rdvl = new QVBoxLayout(route_detail_);
    rdvl->setContentsMargins(10, 10, 10, 10);
    rdvl->setSpacing(4);

    auto* rd_title = new QLabel("SELECTED ROUTE", route_detail_);
    rd_title->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
                                .arg(ui::colors::AMBER())
                                .arg(ui::fonts::DATA_FAMILY));
    rdvl->addWidget(rd_title);

    rd_name_ = new QLabel(route_detail_);
    rd_name_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; font-family:%2;")
                                .arg(ui::colors::INFO())
                                .arg(ui::fonts::DATA_FAMILY));
    rdvl->addWidget(rd_name_);
    rd_value_ = new QLabel(route_detail_);
    rd_value_->setStyleSheet(detail_text_ss());
    rdvl->addWidget(rd_value_);
    rd_status_ = new QLabel(route_detail_);
    rd_status_->setStyleSheet(detail_text_ss());
    rdvl->addWidget(rd_status_);
    rd_vessels_ = new QLabel(route_detail_);
    rd_vessels_->setStyleSheet(detail_text_ss());
    rdvl->addWidget(rd_vessels_);
    vl->addWidget(route_detail_);

    // ── System Status ──────────────────────────────────────────────────────
    vl->addSpacing(8);
    not_found_label_ = new QLabel(content);
    not_found_label_->setVisible(false);
    not_found_label_->setWordWrap(true);
    not_found_label_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;"
                                            "padding:6px; border:1px solid %3; background:%4; border-radius:2px;")
                                        .arg(ui::colors::WARNING())
                                        .arg(ui::fonts::DATA_FAMILY)
                                        .arg(ui::colors::BORDER_MED())
                                        .arg(ui::colors::BG_SURFACE()));
    vl->addWidget(not_found_label_);

    // Suppressed: hardcoded "system status" placeholders (AIS Transponders /
    // Satellite Imagery / 10 corridors / 13 SATs / 6 ports) — none of these
    // figures come from the API. Restore once a real backend exposes them.
    QVector<QPair<QString, QString>> statuses;
    for (const auto& [text, color] : statuses) {
        auto* lbl = new QLabel(QString::fromUtf8("\u25CF ") + text, content);
        lbl->setStyleSheet(QString("color:%1; font-size:8px; font-family:%2;").arg(color).arg(ui::fonts::DATA_FAMILY));
        vl->addWidget(lbl);
    }

    auto* cls = new QLabel("CLASSIFIED — AUTHORIZED PERSONNEL ONLY", content);
    cls->setWordWrap(true);
    cls->setAlignment(Qt::AlignCenter);
    cls->setStyleSheet(QString("color:%1; font-size:8px; font-family:%2; font-weight:700;"
                               "padding:8px; border:1px solid %3; background:%4; border-radius:2px;")
                           .arg(ui::colors::WARNING())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::colors::BORDER_MED())
                           .arg(ui::colors::BG_SURFACE()));
    vl->addWidget(cls);

    vl->addStretch();
    scroll->setWidget(content);

    auto* outer = new QVBoxLayout(panel);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
    return panel;
}

// ── Status Bar ───────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(24);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    auto s =
        QString("color:%1; font-size:8px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY()).arg(ui::fonts::DATA_FAMILY);
    auto sv = QString("color:%1; font-size:8px; font-weight:700; font-family:%2;")
                  .arg(ui::colors::TEXT_PRIMARY())
                  .arg(ui::fonts::DATA_FAMILY);

    auto* lbl1 = new QLabel("SOURCE:", bar);
    lbl1->setStyleSheet(s);
    source_value_ = new QLabel("—", bar);
    source_value_->setStyleSheet(sv);
    hl->addWidget(lbl1);
    hl->addWidget(source_value_);

    auto* lbl_rec = new QLabel("RECORDS:", bar);
    lbl_rec->setStyleSheet(s);
    records_value_ = new QLabel("—", bar);
    records_value_->setStyleSheet(sv);
    hl->addWidget(lbl_rec);
    hl->addWidget(records_value_);

    auto* lbl2 = new QLabel("REFRESH:", bar);
    lbl2->setStyleSheet(s);
    auto* val2 = new QLabel("5 MIN", bar);
    val2->setStyleSheet(sv);
    hl->addWidget(lbl2);
    hl->addWidget(val2);

    hl->addStretch();

    status_label_ = new QLabel("READY", bar);
    hl->addWidget(status_label_);

    return bar;
}

// ── Populate routes table ────────────────────────────────────────────────────
void MaritimeScreen::populate_routes_table() {
    routes_table_->setRowCount(routes_.size());
    for (int i = 0; i < routes_.size(); ++i) {
        const auto& r = routes_[i];
        auto* name_item = new QTableWidgetItem(r.name);
        name_item->setForeground(QBrush(QColor(ui::colors::INFO.get())));
        routes_table_->setItem(i, 0, name_item);
        // The API doesn't expose trade-volume in $ — repurpose the middle
        // column for the live vessel count derived from the loaded set.
        auto* count_item = new QTableWidgetItem(QString::number(r.vessels));
        count_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        routes_table_->setItem(i, 1, count_item);
        auto* status_item = new QTableWidgetItem(r.status.isEmpty() ? QStringLiteral("-") : r.status.toUpper());
        status_item->setForeground(route_status_color(r.status));
        routes_table_->setItem(i, 2, status_item);
    }
    routes_table_->resizeColumnsToContents();
}

// ── Actions ──────────────────────────────────────────────────────────────────
} // namespace fincept::screens
