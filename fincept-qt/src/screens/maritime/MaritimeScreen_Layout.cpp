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
    bar->setFixedHeight(46);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(10);

    // Leading amber accent bar.
    auto* accent = new QLabel(bar);
    accent->setFixedSize(4, 18);
    accent->setStyleSheet(QString("background:%1; border-radius:2px;").arg(C(ui::colors::AMBER)));
    hl->addWidget(accent);

    brand_label_ = new QLabel(tr("FINCEPT MARITIME INTELLIGENCE"), bar);
    hl->addWidget(brand_label_);

    hl->addStretch(1);

    vessel_count_label_ = new QLabel(tr("— VESSELS"), bar);
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

    global_btn_ = new QPushButton(tr("GLOBAL VIEW (200 VESSELS)"), panel);
    global_btn_->setCursor(Qt::PointingHandCursor);
    global_btn_->setStyleSheet(btn_primary_ss());
    connect(global_btn_, &QPushButton::clicked, this, &MaritimeScreen::load_global_sample);
    vl->addWidget(global_btn_);

    load_btn_ = new QPushButton(tr("LOAD VESSELS IN AREA"), panel);
    load_btn_->setCursor(Qt::PointingHandCursor);
    load_btn_->setStyleSheet(btn_outline_ss());
    connect(load_btn_, &QPushButton::clicked, this, &MaritimeScreen::on_load_vessels);
    vl->addWidget(load_btn_);

    // ── Draw-on-map toolbar ─────────────────────────────────────────────────
    //
    // Square / Circle draw tools + Clear. The user can drop multiple shapes;
    // each finalized shape triggers on_shapes_changed which loads the union
    // bbox and filters returned vessels to those inside any drawn shape.
    draw_title_ = new QLabel(tr("DRAW AREA ON MAP"), panel);
    draw_title_->setStyleSheet(section_label_ss());
    vl->addWidget(draw_title_);

    auto* draw_row = new QHBoxLayout;
    draw_row->setSpacing(6);
    sq_btn_ = new QPushButton(tr("◻ SQUARE"), panel);
    ci_btn_ = new QPushButton(tr("◯ CIRCLE"), panel);
    clr_btn_ = new QPushButton(tr("✕ CLEAR"), panel);
    auto* sq_btn = sq_btn_;
    auto* ci_btn = ci_btn_;
    auto* clr_btn = clr_btn_;
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

    intel_title_ = new QLabel(tr("FLEET INTELLIGENCE"), panel);
    intel_title_->setStyleSheet(section_label_ss());
    vl->addWidget(intel_title_);

    auto* grid = new QGridLayout;
    grid->setSpacing(6);

    // `label` is the display caption (translatable); `obj_key` is a stable
    // identifier kept untranslated so any objectName-based lookup is robust.
    // `cap_out` / `val_out` hand back the caption + value labels.
    auto make_stat = [&](const QString& label, const QString& obj_key, const QString& value,
                         const ui::ColorToken& val_color, QLabel** cap_out, QLabel** val_out) {
        auto* box = new QWidget(panel);
        box->setObjectName(QStringLiteral("mtStatBox"));
        // Scope to the box so the border doesn't cascade onto the child labels.
        box->setStyleSheet(QString("#mtStatBox { background:%1; border:1px solid %2; border-radius:2px; }")
                               .arg(C(ui::colors::BG_BASE), C(ui::colors::BORDER_MED)));
        auto* bvl = new QVBoxLayout(box);
        bvl->setContentsMargins(10, 8, 10, 8);
        bvl->setSpacing(3);
        auto* lbl = new QLabel(label, box);
        lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; font-family:%2; letter-spacing:0.5px;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::DATA_FAMILY));
        auto* val = new QLabel(value, box);
        val->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700; font-family:%2;")
                               .arg(C(val_color))
                               .arg(ui::fonts::DATA_FAMILY));
        val->setObjectName(obj_key);
        bvl->addWidget(lbl);
        bvl->addWidget(val);
        if (cap_out) *cap_out = lbl;
        if (val_out) *val_out = val;
        return box;
    };

    // All stats start at "—" and are computed from the loaded vessel set only.
    // No hardcoded trade-volume / satellite figures — the API doesn't expose
    // those, so we don't fabricate them. LOADED = vessels actually on the map;
    // IN REGION = the server's reported total for the searched area (these
    // differ, so they're labelled distinctly to avoid the "200 vs 5.2K" confusion).
    grid->addWidget(make_stat(tr("LOADED"), QStringLiteral("stat_vessels"), "—",
                              ui::colors::AMBER, &stat_vessels_cap_, &stat_vessels_), 0, 0);
    grid->addWidget(make_stat(tr("IN REGION"), QStringLiteral("stat_displayed"), "—",
                              ui::colors::TEXT_PRIMARY, &stat_displayed_cap_, &stat_displayed_), 0, 1);
    grid->addWidget(make_stat(tr("MOVING"), QStringLiteral("stat_moving"), "—",
                              ui::colors::POSITIVE, &stat_moving_cap_, &stat_moving_), 1, 0);
    grid->addWidget(make_stat(tr("AVG SPEED"), QStringLiteral("stat_speed"), "—",
                              ui::colors::TEXT_PRIMARY, &stat_speed_cap_, &stat_speed_), 1, 1);
    grid->addWidget(make_stat(tr("ROUTES"), QStringLiteral("stat_routes"), "—",
                              ui::colors::TEXT_PRIMARY, &stat_routes_cap_, &stat_routes_), 2, 0);
    grid->addWidget(make_stat(tr("DEST PORTS"), QStringLiteral("stat_ports"), "—",
                              ui::colors::TEXT_PRIMARY, &stat_ports_cap_, &stat_ports_), 2, 1);

    vl->addLayout(grid);

    vl->addSpacing(8);
    routes_title_ = new QLabel(tr("TRADE CORRIDORS"), panel);
    routes_title_->setStyleSheet(section_label_ss());
    vl->addWidget(routes_title_);

    routes_table_ = new QTableWidget(panel);
    routes_table_->setColumnCount(3);
    routes_table_->setHorizontalHeaderLabels({tr("Route"), tr("Vessels"), tr("Status")});
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

    // ── Map toolbar ─────────────────────────────────────────────────────────
    // Command-bar styled header above the map: accent + title on the left, a
    // BASEMAP selector on the right. The combo is populated once the map widget
    // exists (below) so its current basemap drives the initial selection.
    auto* header = new QWidget(panel);
    header->setObjectName("mtMapToolbar");
    header->setFixedHeight(36);
    // Flat, even background matching the panels (no lighter "raised" strip) with
    // a single hairline rule separating it from the map. WA_StyledBackground
    // guarantees the whole rect fills uniformly.
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setStyleSheet(QString("#mtMapToolbar { background:%1; border-bottom:1px solid %2; }")
                              .arg(C(ui::colors::BG_BASE), C(ui::colors::BORDER_DIM)));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    hhl->setSpacing(8);

    auto* accent = new QLabel(header);
    accent->setFixedSize(3, 14);
    accent->setStyleSheet(QString("background:%1; border-radius:1px;").arg(C(ui::colors::AMBER)));
    hhl->addWidget(accent);

    center_title_ = new QLabel(tr("LIVE VESSEL MAP"), header);
    center_title_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                             .arg(ui::colors::AMBER())
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(center_title_);
    hhl->addStretch();

    // Manual refresh + visible AUTO-refresh toggle (off by default so it never
    // silently burns API credits — the user opts in).
    refresh_btn_ = new QPushButton(tr("⟳ REFRESH"), header);
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(toolbar_btn_ss());
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() { do_refresh(); });
    hhl->addWidget(refresh_btn_);

    auto_refresh_btn_ = new QPushButton(tr("AUTO ⟳ OFF"), header);
    auto_refresh_btn_->setCursor(Qt::PointingHandCursor);
    auto_refresh_btn_->setCheckable(true);
    auto_refresh_btn_->setStyleSheet(toolbar_btn_ss());
    connect(auto_refresh_btn_, &QPushButton::toggled, this, [this](bool on) {
        auto_refresh_btn_->setText(on ? tr("AUTO ⟳ 5m") : tr("AUTO ⟳ OFF"));
        if (on) refresh_timer_->start();
        else    refresh_timer_->stop();
    });
    hhl->addWidget(auto_refresh_btn_);

    basemap_cap_ = new QLabel(tr("BASEMAP"), header);
    basemap_cap_->setStyleSheet(tiny_label_ss());
    hhl->addWidget(basemap_cap_);

    map_type_combo_ = new QComboBox(header);
    map_type_combo_->setCursor(Qt::PointingHandCursor);
    map_type_combo_->setFixedWidth(132);
    map_type_combo_->setStyleSheet(combo_ss());
    hhl->addWidget(map_type_combo_);

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

    // Populate + wire the basemap selector now that the map exists. The map
    // defaults to SATELLITE; switching is instant and preserves camera/pins.
    if (map_type_combo_) {
        map_type_combo_->addItems(fincept::ui::WorldMapWidget::basemap_labels());
        map_type_combo_->setCurrentIndex(map_widget_->current_basemap_index());
        connect(map_type_combo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
            if (map_widget_)
                map_widget_->set_basemap(idx);
        });
    }

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
        if (sr_imo_)      sr_imo_->setText(tr("IMO: %1").arg(v.imo));
        if (sr_position_) sr_position_->setText(tr("Position: %1, %2")
                                                    .arg(v.latitude, 0, 'f', 4)
                                                    .arg(v.longitude, 0, 'f', 4));
        if (sr_speed_)    sr_speed_->setText(tr("Speed: %1 kn").arg(v.speed, 0, 'f', 1));
        if (sr_from_)     sr_from_->setText(tr("From: %1").arg(v.from_port.isEmpty() ? QStringLiteral("—") : v.from_port));
        if (sr_to_)       sr_to_->setText(tr("To: %1").arg(v.to_port.isEmpty() ? QStringLiteral("—") : v.to_port));
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
        {tr("Name"), tr("IMO"), tr("Lat"), tr("Lng"), tr("Speed (kn)"), tr("Angle"),
         tr("From"), tr("To"), tr("Progress")});
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
    search_title_ = new QLabel(tr("VESSEL SEARCH"), content);
    search_title_->setStyleSheet(section_label_ss());
    vl->addWidget(search_title_);

    imo_cap_ = new QLabel(tr("IMO NUMBER"), content);
    imo_cap_->setStyleSheet(tiny_label_ss());
    vl->addWidget(imo_cap_);

    imo_edit_ = new QLineEdit(content);
    imo_edit_->setPlaceholderText(tr("e.g. 9344745"));
    imo_edit_->setStyleSheet(input_ss());
    connect(imo_edit_, &QLineEdit::returnPressed, this, &MaritimeScreen::on_search_vessel);
    connect(imo_edit_, &QLineEdit::textChanged, this,
            [this](const QString&) { fincept::ScreenStateManager::instance().notify_changed(this); });
    vl->addWidget(imo_edit_);

    track_btn_ = new QPushButton(tr("TRACK"), content);
    track_btn_->setCursor(Qt::PointingHandCursor);
    track_btn_->setStyleSheet(btn_primary_ss());
    connect(track_btn_, &QPushButton::clicked, this, &MaritimeScreen::on_search_vessel);
    vl->addWidget(track_btn_);

    history_btn_ = new QPushButton(tr("VOYAGE HISTORY"), content);
    history_btn_->setCursor(Qt::PointingHandCursor);
    history_btn_->setStyleSheet(btn_outline_ss());
    connect(history_btn_, &QPushButton::clicked, this, [this]() {
        auto imo = imo_edit_->text().trimmed();
        if (imo.isEmpty())
            return;
        set_status(tr("LOADING HISTORY..."), ui::colors::AMBER);
        show_map_loading(tr("LOADING HISTORY"));
        MaritimeService::instance().get_vessel_history(imo);
    });
    vl->addWidget(history_btn_);

    // Search result card
    search_result_card_ = new QWidget(content);
    search_result_card_->setObjectName(QStringLiteral("mtSearchCard"));
    search_result_card_->setVisible(false);
    auto* srvl = new QVBoxLayout(search_result_card_);
    srvl->setContentsMargins(10, 10, 10, 10);
    srvl->setSpacing(4);

    sr_name_ = new QLabel(search_result_card_);
    sr_name_->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; font-family:%2;")
                                .arg(ui::colors::AMBER())
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
    place_title_ = new QLabel(tr("PLACE SEARCH"), content);
    place_title_->setStyleSheet(section_label_ss());
    vl->addWidget(place_title_);

    place_cap_ = new QLabel(tr("PLACE NAME"), content);
    place_cap_->setStyleSheet(tiny_label_ss());
    vl->addWidget(place_cap_);

    place_query_edit_ = new QLineEdit(content);
    place_query_edit_->setPlaceholderText(tr("e.g. Strait of Malacca"));
    place_query_edit_->setStyleSheet(input_ss());
    auto run_place_search = [this]() {
        const QString q = place_query_edit_->text().trimmed();
        if (q.isEmpty()) return;
        if (place_status_) place_status_->setText(tr("Searching…"));
        services::maritime::GeocodingService::instance().search(q);
    };
    connect(place_query_edit_, &QLineEdit::returnPressed, this, run_place_search);

    // Live typeahead: a completer popup fed by debounced geocoder results.
    place_completer_model_ = new QStringListModel(this);
    place_completer_ = new QCompleter(place_completer_model_, this);
    place_completer_->setCaseSensitivity(Qt::CaseInsensitive);
    place_completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    place_completer_->setMaxVisibleItems(8);
    place_query_edit_->setCompleter(place_completer_);
    if (auto* popup = place_completer_->popup()) {
        popup->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3;"
                                     "font-family:%4; font-size:%5px; outline:0;"
                                     "selection-background-color:%6; selection-color:%7;")
                                 .arg(C(ui::colors::BG_RAISED), C(ui::colors::TEXT_PRIMARY),
                                      C(ui::colors::BORDER_MED), F(ui::fonts::DATA_FAMILY))
                                 .arg(ui::fonts::SMALL)
                                 .arg(C(ui::colors::BG_HOVER), C(ui::colors::AMBER)));
    }
    // Picking a suggestion loads vessels for that place's bbox.
    connect(place_completer_, QOverload<const QString&>::of(&QCompleter::activated), this,
            [this](const QString& text) {
                for (const auto& p : place_results_) {
                    if (p.display_name == text) {
                        run_area_search(p.min_lat, p.max_lat, p.min_lng, p.max_lng);
                        return;
                    }
                }
            });
    place_debounce_ = new QTimer(this);
    place_debounce_->setSingleShot(true);
    place_debounce_->setInterval(350);
    connect(place_debounce_, &QTimer::timeout, this, [this]() {
        const QString q = place_query_edit_->text().trimmed();
        if (q.size() < 3) return;
        if (place_status_) place_status_->setText(tr("Searching…"));
        services::maritime::GeocodingService::instance().search(q);
    });
    // textEdited (not textChanged) so programmatic fills don't trigger searches.
    connect(place_query_edit_, &QLineEdit::textEdited, this,
            [this](const QString&) { place_debounce_->start(); });
    vl->addWidget(place_query_edit_);

    place_btn_ = new QPushButton(tr("SEARCH PLACES"), content);
    place_btn_->setCursor(Qt::PointingHandCursor);
    place_btn_->setStyleSheet(btn_primary_ss());
    connect(place_btn_, &QPushButton::clicked, this, run_place_search);
    vl->addWidget(place_btn_);

    place_status_ = new QLabel(content);
    place_status_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
                                     .arg(ui::colors::TEXT_TERTIARY())
                                     .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(place_status_);

    place_table_ = new QTableWidget(content);
    place_table_->setColumnCount(2);
    place_table_->setHorizontalHeaderLabels({tr("Place"), tr("Type")});
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
    area_toggle_ = new QPushButton(tr("▸ ADVANCED: RAW BBOX"), content);
    auto* area_toggle = area_toggle_;
    area_toggle->setCursor(Qt::PointingHandCursor);
    area_toggle->setCheckable(true);
    area_toggle->setStyleSheet(btn_outline_ss());
    vl->addWidget(area_toggle);

    advanced_area_box_ = new QWidget(content);
    advanced_area_box_->setVisible(false);
    auto* area_vl = new QVBoxLayout(advanced_area_box_);
    area_vl->setContentsMargins(0, 4, 0, 0);
    area_vl->setSpacing(8);

    auto make_coord = [&](const QString& label, double def_val, QLabel** cap_out) {
        auto* lbl = new QLabel(label, advanced_area_box_);
        lbl->setStyleSheet(tiny_label_ss());
        area_vl->addWidget(lbl);
        auto* spin = new QDoubleSpinBox(advanced_area_box_);
        spin->setRange(-180, 180);
        spin->setDecimals(4);
        spin->setValue(def_val);
        spin->setStyleSheet(input_ss());
        area_vl->addWidget(spin);
        if (cap_out) *cap_out = lbl;
        return spin;
    };

    // Spinners start at zero — the user supplies a real bbox before the
    // first call (or restore_state injects the last-used coords).
    area_min_lat_ = make_coord(tr("MIN LATITUDE"), 0.0, &coord_min_lat_cap_);
    area_max_lat_ = make_coord(tr("MAX LATITUDE"), 0.0, &coord_max_lat_cap_);
    area_min_lng_ = make_coord(tr("MIN LONGITUDE"), 0.0, &coord_min_lng_cap_);
    area_max_lng_ = make_coord(tr("MAX LONGITUDE"), 0.0, &coord_max_lng_cap_);

    area_btn_ = new QPushButton(tr("SEARCH AREA"), advanced_area_box_);
    area_btn_->setCursor(Qt::PointingHandCursor);
    area_btn_->setStyleSheet(btn_primary_ss());
    connect(area_btn_, &QPushButton::clicked, this, [this]() {
        run_area_search(area_min_lat_->value(), area_max_lat_->value(),
                        area_min_lng_->value(), area_max_lng_->value());
    });
    area_vl->addWidget(area_btn_);

    connect(area_toggle, &QPushButton::toggled, this, [this](bool on) {
        advanced_area_box_->setVisible(on);
        area_toggle_->setText(on ? tr("▾ ADVANCED: RAW BBOX") : tr("▸ ADVANCED: RAW BBOX"));
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
    ports_title_ = new QLabel(tr("PORTS"), content);
    ports_title_->setStyleSheet(section_label_ss());
    vl->addWidget(ports_title_);

    ports_cap_ = new QLabel(tr("PORT NAME"), content);
    ports_cap_->setStyleSheet(tiny_label_ss());
    vl->addWidget(ports_cap_);

    ports_query_edit_ = new QLineEdit(content);
    ports_query_edit_->setPlaceholderText(tr("e.g. Rotterdam"));
    ports_query_edit_->setStyleSheet(input_ss());
    auto run_name_search = [this]() {
        const QString q = ports_query_edit_->text().trimmed();
        if (q.isEmpty()) return;
        if (ports_status_) ports_status_->setText(tr("Searching…"));
        PortsCatalog::instance().search_by_name(q);
    };
    connect(ports_query_edit_, &QLineEdit::returnPressed, this, run_name_search);

    // Live typeahead for ports — same pattern as place search.
    ports_completer_model_ = new QStringListModel(this);
    ports_completer_ = new QCompleter(ports_completer_model_, this);
    ports_completer_->setCaseSensitivity(Qt::CaseInsensitive);
    ports_completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    ports_completer_->setMaxVisibleItems(8);
    ports_query_edit_->setCompleter(ports_completer_);
    if (auto* popup = ports_completer_->popup()) {
        popup->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3;"
                                     "font-family:%4; font-size:%5px; outline:0;"
                                     "selection-background-color:%6; selection-color:%7;")
                                 .arg(C(ui::colors::BG_RAISED), C(ui::colors::TEXT_PRIMARY),
                                      C(ui::colors::BORDER_MED), F(ui::fonts::DATA_FAMILY))
                                 .arg(ui::fonts::SMALL)
                                 .arg(C(ui::colors::BG_HOVER), C(ui::colors::AMBER)));
    }
    // Picking a port suggestion plots it on the map (amber pin) and flies there.
    connect(ports_completer_, QOverload<const QString&>::of(&QCompleter::activated), this,
            [this](const QString& text) {
                if (!map_widget_) return;
                for (const auto& p : port_results_) {
                    if (p.name == text) {
                        fincept::ui::MapPin pin;
                        pin.latitude  = p.latitude;
                        pin.longitude = p.longitude;
                        pin.label     = QString("%1 (%2)").arg(p.name, p.locode.isEmpty() ? p.country : p.locode);
                        pin.color     = QColor(ui::colors::AMBER.get());
                        pin.radius    = 6.0;
                        pin.id        = -1;
                        map_widget_->add_pin(pin);
                        map_widget_->fly_to(p.latitude, p.longitude);
                        return;
                    }
                }
            });
    ports_debounce_ = new QTimer(this);
    ports_debounce_->setSingleShot(true);
    ports_debounce_->setInterval(350);
    connect(ports_debounce_, &QTimer::timeout, this, [this]() {
        const QString q = ports_query_edit_->text().trimmed();
        if (q.size() < 3) return;
        if (ports_status_) ports_status_->setText(tr("Searching…"));
        PortsCatalog::instance().search_by_name(q);
    });
    connect(ports_query_edit_, &QLineEdit::textEdited, this,
            [this](const QString&) { ports_debounce_->start(); });
    vl->addWidget(ports_query_edit_);

    ports_btn_ = new QPushButton(tr("SEARCH PORTS"), content);
    ports_btn_->setCursor(Qt::PointingHandCursor);
    ports_btn_->setStyleSheet(btn_primary_ss());
    connect(ports_btn_, &QPushButton::clicked, this, run_name_search);
    vl->addWidget(ports_btn_);

    ports_in_view_btn_ = new QPushButton(tr("PORTS IN AREA"), content);
    ports_in_view_btn_->setCursor(Qt::PointingHandCursor);
    ports_in_view_btn_->setStyleSheet(btn_outline_ss());
    connect(ports_in_view_btn_, &QPushButton::clicked, this, [this]() {
        if (!area_min_lat_ || !area_max_lat_ || !area_min_lng_ || !area_max_lng_) return;
        const double mn_lat = area_min_lat_->value();
        const double mx_lat = area_max_lat_->value();
        const double mn_lng = area_min_lng_->value();
        const double mx_lng = area_max_lng_->value();
        if (mx_lat <= mn_lat || mx_lng <= mn_lng) {
            if (ports_status_) ports_status_->setText(tr("Set a valid bbox in AREA SEARCH first."));
            return;
        }
        if (ports_status_) ports_status_->setText(tr("Searching…"));
        PortsCatalog::instance().search_by_bbox(mn_lat, mx_lat, mn_lng, mx_lng);
    });
    vl->addWidget(ports_in_view_btn_);

    ports_status_ = new QLabel(content);
    ports_status_->setStyleSheet(QString("color:%1; font-size:9px; font-family:%2;")
                                     .arg(ui::colors::TEXT_TERTIARY())
                                     .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(ports_status_);

    ports_table_ = new QTableWidget(content);
    ports_table_->setColumnCount(3);
    ports_table_->setHorizontalHeaderLabels({tr("Name"), tr("Country"), tr("Source")});
    ports_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ports_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    ports_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    ports_table_->verticalHeader()->setVisible(false);
    ports_table_->horizontalHeader()->setStretchLastSection(true);
    ports_table_->setMinimumHeight(160);
    ports_table_->setStyleSheet(table_ss());
    // Click → drop an AMBER pin on the existing world map to distinguish from
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
        pin.color     = QColor(ui::colors::AMBER.get());
        pin.radius    = 6.0;
        pin.id        = -1;
        map_widget_->add_pin(pin);
        map_widget_->fly_to(p.latitude, p.longitude);
    });
    vl->addWidget(ports_table_);

    // ── Route Detail ───────────────────────────────────────────────────────
    vl->addSpacing(8);
    route_detail_ = new QWidget(content);
    route_detail_->setObjectName(QStringLiteral("mtRouteCard"));
    route_detail_->setVisible(false);
    auto* rdvl = new QVBoxLayout(route_detail_);
    rdvl->setContentsMargins(10, 10, 10, 10);
    rdvl->setSpacing(4);

    rd_title_ = new QLabel(tr("SELECTED ROUTE"), route_detail_);
    rd_title_->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;")
                                .arg(ui::colors::AMBER())
                                .arg(ui::fonts::DATA_FAMILY));
    rdvl->addWidget(rd_title_);

    rd_name_ = new QLabel(route_detail_);
    rd_name_->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700; font-family:%2;")
                                .arg(ui::colors::AMBER())
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
    not_found_label_->setStyleSheet(QString("color:%1; font-size:11px; font-family:%2;"
                                            "padding:6px; border:1px solid %3; background:%4; border-radius:2px;")
                                        .arg(ui::colors::AMBER())
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

    classified_footer_ = new QLabel(tr("CLASSIFIED — AUTHORIZED PERSONNEL ONLY"), content);
    classified_footer_->setWordWrap(true);
    classified_footer_->setAlignment(Qt::AlignCenter);
    classified_footer_->setStyleSheet(QString("color:%1; font-size:10px; font-family:%2; font-weight:700;"
                               "padding:8px; border:1px solid %3; background:%4; border-radius:2px;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::colors::BORDER_MED())
                           .arg(ui::colors::BG_SURFACE()));
    vl->addWidget(classified_footer_);

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
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    auto s =
        QString("color:%1; font-size:10px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY()).arg(ui::fonts::DATA_FAMILY);
    auto sv = QString("color:%1; font-size:10px; font-weight:700; font-family:%2;")
                  .arg(ui::colors::TEXT_PRIMARY())
                  .arg(ui::fonts::DATA_FAMILY);

    sb_source_cap_ = new QLabel(tr("SOURCE:"), bar);
    sb_source_cap_->setStyleSheet(s);
    source_value_ = new QLabel("—", bar);
    source_value_->setStyleSheet(sv);
    hl->addWidget(sb_source_cap_);
    hl->addWidget(source_value_);

    sb_records_cap_ = new QLabel(tr("RECORDS:"), bar);
    sb_records_cap_->setStyleSheet(s);
    records_value_ = new QLabel("—", bar);
    records_value_->setStyleSheet(sv);
    hl->addWidget(sb_records_cap_);
    hl->addWidget(records_value_);

    // Real last-update clock (set on each successful load) — replaces the old
    // hardcoded "5 MIN" refresh label.
    sb_refresh_cap_ = new QLabel(tr("UPDATED:"), bar);
    sb_refresh_cap_->setStyleSheet(s);
    sb_refresh_val_ = new QLabel(QStringLiteral("—"), bar);
    sb_refresh_val_->setStyleSheet(sv);
    hl->addWidget(sb_refresh_cap_);
    hl->addWidget(sb_refresh_val_);

    hl->addStretch();

    status_label_ = new QLabel(tr("READY"), bar);
    hl->addWidget(status_label_);

    return bar;
}

// ── Populate routes table ────────────────────────────────────────────────────
void MaritimeScreen::populate_routes_table() {
    routes_table_->setRowCount(routes_.size());
    for (int i = 0; i < routes_.size(); ++i) {
        const auto& r = routes_[i];
        auto* name_item = new QTableWidgetItem(r.name);
        name_item->setForeground(QBrush(QColor(ui::colors::AMBER.get())));
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
