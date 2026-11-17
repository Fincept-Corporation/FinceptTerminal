// src/screens/maritime/MaritimeScreen_Layout.cpp
//
// UI construction: build_ui, build_top_bar, build_left_panel,
// build_center_panel, build_right_panel, build_status_bar, populate_routes_table.
//
// Part of the partial-class split of MaritimeScreen.cpp.

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/maritime/MaritimeScreen.h"
#include "screens/maritime/MaritimeScreen_internal.h"
#include "services/maritime/GeocodingService.h"
#include "services/maritime/MaritimeService.h"
#include "services/maritime/PortsCatalog.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/charts/RangeBar.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/Card.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/PlanHint.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/LoadingOverlay.h"
#include "ui/widgets/StatTile.h"
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
#include <QStackedWidget>
#include <QTabWidget>

#include <algorithm>
#include <utility>

namespace fincept::screens {

using namespace fincept::services::maritime;
using namespace fincept::screens::maritime_internal;

// ── Build UI ─────────────────────────────────────────────────────────────────
void MaritimeScreen::build_ui() {
    setObjectName(QStringLiteral("maritimeScreen"));
    setAttribute(Qt::WA_StyledBackground, true); // paint #maritimeScreen's background
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
    bar->setObjectName(QStringLiteral("mtTopBar"));
    bar->setMinimumHeight(46);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(10);

    // Leading amber accent bar.
    auto* accent = new QLabel(bar);
    accent->setObjectName(QStringLiteral("mtAccent"));
    accent->setFixedSize(4, 18);
    hl->addWidget(accent);

    brand_label_ = new QLabel(tr("FINCEPT MARITIME INTELLIGENCE"), bar);
    brand_label_->setObjectName(QStringLiteral("mtBrand"));
    hl->addWidget(brand_label_);

    hl->addStretch(1);

    // Every marine call is metered (area search 8 credits, history 5, position 1);
    // the balance after the last call is shown here, with PLANS when it runs low.
    credits_label_ = new QLabel(tr("CREDITS: —"), bar);
    credits_label_->setObjectName(QStringLiteral("mtCredits"));
    credits_label_->setToolTip(tr("Fincept API credit balance after the last vessel request"));
    hl->addWidget(credits_label_);
    // What the plan changes for vessel data (half price on Pro) — hidden until
    // the server has sent its plan table.
    auto* plan_hint = new ui::PlanHintLabel(QStringLiteral("maritime"), bar);
    plan_hint->set_compact(true);
    hl->addWidget(plan_hint);

    plans_btn_ = new QPushButton(tr("PLANS"), bar);
    plans_btn_->setObjectName(QStringLiteral("mtPlansBtn"));
    plans_btn_->setCursor(Qt::PointingHandCursor);
    plans_btn_->setToolTip(tr("Credits are running low — see Plans & Pricing"));
    plans_btn_->hide();
    connect(plans_btn_, &QPushButton::clicked, this, [this]() { ui::billing::open_plans(this); });
    hl->addWidget(plans_btn_);

    vessel_count_label_ = new QLabel(tr("— VESSELS"), bar);
    vessel_count_label_->setObjectName(QStringLiteral("mtVesselCount"));
    hl->addWidget(vessel_count_label_);

    return bar;
}

// ── Left Panel ───────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_left_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("mtLeftPanel"));
    panel->setFixedWidth(std::max(260, mt_text_width(30, -1, QFont::Bold) + 24));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    global_btn_ = new QPushButton(tr("GLOBAL VIEW (200 VESSELS)"), panel);
    global_btn_->setCursor(Qt::PointingHandCursor);
    mt_set_role(global_btn_, "primary");
    connect(global_btn_, &QPushButton::clicked, this, &MaritimeScreen::load_global_sample);
    vl->addWidget(global_btn_);

    load_btn_ = new QPushButton(tr("LOAD VESSELS IN AREA"), panel);
    load_btn_->setProperty("fincept_agent_risk", "spend"); // agents: loads vessels (credits)
    load_btn_->setCursor(Qt::PointingHandCursor);
    mt_set_role(load_btn_, "outline");
    connect(load_btn_, &QPushButton::clicked, this, &MaritimeScreen::on_load_vessels);
    vl->addWidget(load_btn_);

    // ── Draw-on-map toolbar ─────────────────────────────────────────────────
    //
    // Square / Circle draw tools + Clear. The user can drop multiple shapes;
    // each finalized shape triggers on_shapes_changed which loads the union
    // bbox and filters returned vessels to those inside any drawn shape.
    draw_title_ = new QLabel(tr("DRAW AREA ON MAP"), panel);
    mt_set_role(draw_title_, "section");
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
        mt_set_role(b, "outline");
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
        if (!map_widget_)
            return;
        if (on)
            ci_btn->setChecked(false);
        map_widget_->set_draw_mode(on ? fincept::ui::MapDrawMode::Rectangle : fincept::ui::MapDrawMode::None);
    });
    connect(ci_btn, &QPushButton::toggled, this, [this, sq_btn](bool on) {
        if (!map_widget_)
            return;
        if (on)
            sq_btn->setChecked(false);
        map_widget_->set_draw_mode(on ? fincept::ui::MapDrawMode::Circle : fincept::ui::MapDrawMode::None);
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
    mt_set_role(intel_title_, "section");
    vl->addWidget(intel_title_);

    auto* grid = new QGridLayout;
    grid->setSpacing(6);

    // `label` is the display caption (translatable); `obj_key` is a stable
    // identifier kept untranslated so any objectName-based lookup is robust.
    // `cap_out` / `val_out` hand back the caption + value labels.
    auto make_stat = [&](const QString& label, const QString& obj_key, const QString& value, const char* tone,
                         QLabel** cap_out, QLabel** val_out) {
        auto* box = new QWidget(panel);
        box->setObjectName(QStringLiteral("mtStatBox"));
        auto* bvl = new QVBoxLayout(box);
        bvl->setContentsMargins(10, 8, 10, 8);
        bvl->setSpacing(3);
        auto* lbl = new QLabel(label, box);
        mt_set_role(lbl, "statCap");
        auto* val = new QLabel(value, box);
        mt_set_role(val, "statVal");
        if (tone)
            val->setProperty("tone", QString::fromLatin1(tone));
        val->setObjectName(obj_key);
        bvl->addWidget(lbl);
        bvl->addWidget(val);
        if (cap_out)
            *cap_out = lbl;
        if (val_out)
            *val_out = val;
        return box;
    };

    // All stats start at "—" and are computed from the loaded vessel set only.
    // No hardcoded trade-volume / satellite figures — the API doesn't expose
    // those, so we don't fabricate them. LOADED = vessels actually on the map;
    // IN REGION = the server's reported total for the searched area (these
    // differ, so they're labelled distinctly to avoid the "200 vs 5.2K" confusion).
    grid->addWidget(make_stat(tr("LOADED"), QStringLiteral("stat_vessels"), "—", "amber", &stat_vessels_cap_,
                              &stat_vessels_),
                    0, 0);
    grid->addWidget(make_stat(tr("IN REGION"), QStringLiteral("stat_displayed"), "—", nullptr, &stat_displayed_cap_, &stat_displayed_),
                    0, 1);
    grid->addWidget(make_stat(tr("MOVING"), QStringLiteral("stat_moving"), "—", "positive", &stat_moving_cap_,
                              &stat_moving_),
                    1, 0);
    grid->addWidget(make_stat(tr("AVG SPEED"), QStringLiteral("stat_speed"), "—", nullptr, &stat_speed_cap_, &stat_speed_),
                    1, 1);
    grid->addWidget(make_stat(tr("ROUTES"), QStringLiteral("stat_routes"), "—", nullptr, &stat_routes_cap_, &stat_routes_),
                    2, 0);
    grid->addWidget(make_stat(tr("DEST PORTS"), QStringLiteral("stat_ports"), "—", nullptr, &stat_ports_cap_, &stat_ports_),
                    2, 1);

    vl->addLayout(grid);

    vl->addSpacing(8);
    routes_title_ = new QLabel(tr("TRADE CORRIDORS"), panel);
    mt_set_role(routes_title_, "section");
    vl->addWidget(routes_title_);

    // Corridors are derived from the loaded vessels' from/to ports; there is no
    // corridor status or trade value in the data, so only the vessel count is shown.
    // The corridors table shares its slot with a designed state until a load
    // gives it rows.
    routes_stack_ = new QStackedWidget(panel);
    routes_table_ = new QTableWidget(routes_stack_);
    routes_table_->setColumnCount(2);
    routes_table_->setHorizontalHeaderLabels({tr("Route"), tr("Vessels")});
    routes_table_->setToolTip(tr("Select a corridor to show only its vessels in the table and on the map"));
    routes_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    routes_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routes_table_->horizontalHeader()->setStretchLastSection(true);
    routes_table_->verticalHeader()->setVisible(false);
    connect(routes_table_, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) { on_route_selected(row); });
    routes_stack_->addWidget(routes_table_);
    routes_state_ = new ui::EmptyState(routes_stack_);
    routes_state_->set_compact(true);
    routes_state_->set_alignment(Qt::AlignTop);
    routes_state_->set_content(QStringLiteral("⇄"), tr("Trade corridors"),
                               tr("Departure → destination port pairs of the loaded vessels, busiest first. Click "
                                  "one to show only its vessels."));
    routes_stack_->addWidget(routes_state_);
    routes_stack_->setCurrentWidget(routes_state_);
    vl->addWidget(routes_stack_, 1);

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
    header->setMinimumHeight(36);
    // Flat, even background matching the panels with a single hairline rule (root
    // sheet). WA_StyledBackground guarantees the whole rect fills uniformly.
    header->setAttribute(Qt::WA_StyledBackground, true);
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    hhl->setSpacing(8);

    auto* accent = new QLabel(header);
    accent->setObjectName(QStringLiteral("mtAccent"));
    accent->setFixedSize(3, 14);
    hhl->addWidget(accent);

    center_title_ = new QLabel(tr("LIVE VESSEL MAP"), header);
    center_title_->setObjectName(QStringLiteral("mtCenterTitle"));
    hhl->addWidget(center_title_);
    hhl->addStretch();
    // "as of 14:02 · refreshing…" of the vessel list (kept by its AutoLoadPane).
    vessels_asof_lbl_ = new QLabel(header);
    mt_set_role(vessels_asof_lbl_, "tiny");
    hhl->addWidget(vessels_asof_lbl_);

    // Manual refresh + visible AUTO-refresh toggle (off by default so it never
    // silently burns API credits — the user opts in).
    refresh_btn_ = new QPushButton(tr("⟳ REFRESH"), header);
    refresh_btn_->setProperty("fincept_agent_risk", "spend"); // agents: reloads vessels (credits)
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    mt_set_role(refresh_btn_, "toolbar");
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() { do_refresh(); });
    hhl->addWidget(refresh_btn_);

    auto_refresh_btn_ = new QPushButton(tr("AUTO ⟳ OFF"), header);
    auto_refresh_btn_->setProperty("fincept_agent_risk", "spend"); // agents: turns on billed auto-refresh
    auto_refresh_btn_->setCursor(Qt::PointingHandCursor);
    auto_refresh_btn_->setCheckable(true);
    mt_set_role(auto_refresh_btn_, "toolbar");
    connect(auto_refresh_btn_, &QPushButton::toggled, this, [this](bool on) {
        auto_refresh_btn_->setText(on ? tr("AUTO ⟳ 5m") : tr("AUTO ⟳ OFF"));
        if (on)
            refresh_timer_->start();
        else
            refresh_timer_->stop();
    });
    hhl->addWidget(auto_refresh_btn_);

    // Reference layers from the bundled (offline) map data.
    ports_layer_btn_ = new QPushButton(tr("PORTS"), header);
    ports_layer_btn_->setCheckable(true);
    ports_layer_btn_->setCursor(Qt::PointingHandCursor);
    ports_layer_btn_->setToolTip(tr("Show the world's major ports (Natural Earth, offline)"));
    mt_set_role(ports_layer_btn_, "toolbar");
    hhl->addWidget(ports_layer_btn_);
    chokepoints_layer_btn_ = new QPushButton(tr("CHOKEPOINTS"), header);
    chokepoints_layer_btn_->setCheckable(true);
    chokepoints_layer_btn_->setCursor(Qt::PointingHandCursor);
    chokepoints_layer_btn_->setToolTip(tr("Show the maritime chokepoints: straits and canals carrying most seaborne trade"));
    mt_set_role(chokepoints_layer_btn_, "toolbar");
    hhl->addWidget(chokepoints_layer_btn_);

    basemap_cap_ = new QLabel(tr("BASEMAP"), header);
    mt_set_role(basemap_cap_, "tiny");
    hhl->addWidget(basemap_cap_);

    map_type_combo_ = new QComboBox(header);
    map_type_combo_->setCursor(Qt::PointingHandCursor);
    map_type_combo_->setFixedWidth(std::max(132, mt_text_width(12, -1, QFont::Bold) + 40));
    hhl->addWidget(map_type_combo_);

    vl->addWidget(header);

    auto* splitter = new QSplitter(Qt::Vertical, panel);
    splitter->setHandleWidth(2);

    map_widget_ = new fincept::ui::WorldMapWidget(splitter);
    map_widget_->setMinimumHeight(280);
    splitter->addWidget(map_widget_);
    // Overlay sits on top of the map and follows its size; it consumes mouse
    // events while visible so the user can't pan a half-loaded map.
    map_loader_ = new fincept::ui::LoadingOverlay(map_widget_);

    // Populate + wire the basemap selector now that the map exists. The map
    // opens on the offline MAP basemap; switching is instant and preserves
    // camera/pins. Basemap, layers and camera are saved with the screen state.
    if (map_type_combo_) {
        map_type_combo_->addItems(fincept::ui::WorldMapWidget::basemap_labels());
        map_type_combo_->setCurrentIndex(map_widget_->current_basemap_index());
        connect(map_type_combo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
            if (map_widget_)
                map_widget_->set_basemap(idx);
        });
    }
    connect(ports_layer_btn_, &QPushButton::toggled, this, [this](bool on) {
        map_widget_->set_reference_layer(fincept::ui::MapLayer::Ports, on);
    });
    connect(chokepoints_layer_btn_, &QPushButton::toggled, this, [this](bool on) {
        map_widget_->set_reference_layer(fincept::ui::MapLayer::Chokepoints, on);
    });
    connect(map_widget_, &fincept::ui::WorldMapWidget::view_changed, this, [this]() {
        // Keep the toolbar in step with a restored view, then persist.
        if (map_type_combo_ && map_type_combo_->currentIndex() != map_widget_->current_basemap_index()) {
            const QSignalBlocker block(map_type_combo_);
            map_type_combo_->setCurrentIndex(map_widget_->current_basemap_index());
        }
        for (auto [btn, layer] : {std::pair{ports_layer_btn_, fincept::ui::MapLayer::Ports},
                                  std::pair{chokepoints_layer_btn_, fincept::ui::MapLayer::Chokepoints}}) {
            if (btn && btn->isChecked() != map_widget_->reference_layer(layer)) {
                const QSignalBlocker block(btn);
                btn->setChecked(map_widget_->reference_layer(layer));
            }
        }
        fincept::ScreenStateManager::instance().notify_changed(this);
    });

    // Drawn-shape changes → load vessels for the union bbox and filter to
    // those inside the shapes.
    connect(map_widget_, &fincept::ui::WorldMapWidget::shapes_changed, this, &MaritimeScreen::on_shapes_changed);

    // Pin click → populate the right-panel SELECTED VESSEL card with the
    // record we cached in update_map. Clicks during a stale map (cached set
    // emptied) silently no-op.
    connect(map_widget_, &fincept::ui::WorldMapWidget::pin_clicked, this, [this](int id) {
        if (id < 0 || id >= rendered_vessels_.size())
            return;
        const auto& v = rendered_vessels_[id];
        // Also syncs the IMO field so the user can hit "VOYAGE HISTORY" right
        // after clicking a pin without retyping.
        show_vessel_card(v);
        // Highlight the corresponding row in the vessels table if it's
        // currently rendered (table may be in a different sort order).
        if (vessels_table_) {
            for (int r = 0; r < vessels_table_->rowCount(); ++r) {
                auto* item = vessels_table_->item(r, 1); // IMO column
                if (item && item->text() == v.imo) {
                    vessels_table_->setCurrentCell(r, 0);
                    vessels_table_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
                    break;
                }
            }
        }
    });

    // Voyage-history summary (history mode only): headline KPIs and the speed
    // profile over the reported fixes, above the per-fix table.
    history_panel_ = new QWidget(splitter);
    history_panel_->setObjectName(QStringLiteral("mtHistoryPanel"));
    auto* hpl = new QVBoxLayout(history_panel_);
    hpl->setContentsMargins(8, 6, 8, 6);
    hpl->setSpacing(6);
    history_kpis_ = new ui::KpiStrip(history_panel_);
    const std::pair<const char*, QString> history_tiles[] = {{"positions", tr("POSITIONS")},
                                                             {"first", tr("FIRST FIX")},
                                                             {"last", tr("LAST FIX")},
                                                             {"avg", tr("AVG SPEED (KN)")},
                                                             {"max", tr("MAX SPEED (KN)")}};
    for (const auto& [key, label] : history_tiles)
        history_kpis_->add_tile(QString::fromLatin1(key), label)->set_compact(true);
    hpl->addWidget(history_kpis_);
    history_chart_ = new ui::TimeSeriesChart(history_panel_);
    history_chart_->set_title(tr("SPEED OVER GROUND (KN)"));
    history_chart_->set_show_mode_buttons(false);
    history_chart_->set_utc(true);
    history_chart_->setMinimumHeight(140);
    hpl->addWidget(history_chart_, 1);
    history_empty_ = new QLabel(history_panel_);
    mt_set_role(history_empty_, "hint");
    history_empty_->setAlignment(Qt::AlignCenter);
    history_empty_->setWordWrap(true);
    history_empty_->hide();
    hpl->addWidget(history_empty_);
    history_panel_->hide();
    splitter->addWidget(history_panel_);

    // The vessel list lives in the shared AutoLoadPane (Manual: 8 credits per
    // area search): the last result of the view with its "as of", else the
    // preview of a paid load; skeleton while loading; failure + RETRY.
    vessels_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::Manual, splitter);
    vessels_table_ = new QTableWidget(vessels_pane_->content_parent());
    vessels_table_->setColumnCount(9);
    vessels_table_->setHorizontalHeaderLabels({tr("Name"), tr("IMO"), tr("Lat"), tr("Lng"), tr("Speed (kn)"),
                                               tr("Angle"), tr("From"), tr("To"), tr("Progress")});
    vessels_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    vessels_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    vessels_table_->setAlternatingRowColors(true);
    vessels_table_->horizontalHeader()->setStretchLastSection(true);
    vessels_table_->verticalHeader()->setVisible(false);
    vessels_table_->setSortingEnabled(true);
    // Click a row to inspect that vessel in the right-hand card (mirrors a map pin
    // click); Enter / double-click additionally flies the map there. Deliberately not
    // currentCellChanged: a reload moves the current cell and would overwrite the card.
    connect(vessels_table_, &QTableWidget::cellClicked, this,
            [this](int row, int) { on_vessel_row_picked(row, false); });
    connect(vessels_table_, &QTableWidget::cellActivated, this,
            [this](int row, int) { on_vessel_row_picked(row, true); });
    vessels_pane_->set_content(vessels_table_);
    vessels_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Table);
    vessels_pane_->skeleton()->set_row_count(8);
    vessels_pane_->set_cost_credits(8, QStringLiteral("maritime")); // list price; a known balance below the plan's price → "Not enough credits" + VIEW PLANS
    vessels_pane_->set_max_age(60 * 60); // older positions → "may be out of date · REFRESH"
    vessels_pane_->set_persist_limit(0); // the service keeps the copy (remembered_vessels)
    vessels_pane_->set_remembered_source(
        [this](const QString& view, QJsonValue* payload, QDateTime* as_of) {
            return remembered_vessels(view, payload, as_of);
        });
    vessels_pane_->set_start([this](quint64 id) { start_vessels_request(id); });
    vessels_pane_->set_cancel([this](quint64 id) {
        if (vessels_request_id_ == id)
            vessels_request_id_ = 0; // the reply still updates map and figures; the pane ignores it
    });
    vessels_pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool from_cache) {
        render_vessels_payload(payload, from_cache);
    });
    vessels_pane_->set_status_label(vessels_asof_lbl_);
    // Free: the bundled ports and chokepoints layers.
    connect(vessels_pane_->preview(), &ui::EmptyState::secondary_clicked, this, [this]() {
        if (ports_layer_btn_)
            ports_layer_btn_->setChecked(true);
        if (chokepoints_layer_btn_)
            chokepoints_layer_btn_->setChecked(true);
    });
    connect(vessels_pane_, &ui::AutoLoadPane::state_changed, this, [this](ui::AutoLoadPane::State st) {
        if (st != ui::AutoLoadPane::State::Preview)
            return;
        // Nothing loaded for this view: say so on the map and in the status bar,
        // and on a first visit (no saved map view) show the free reference layers.
        if (map_widget_ && rendered_vessels_.isEmpty())
            map_widget_->set_overlay_message(tr("Vessels not loaded yet — 8 credits per area search"));
        set_status(tr("NOT LOADED"), "muted");
        if (status_label_)
            status_label_->setToolTip(tr("Vessel positions are loaded on request (8 credits per area search)."));
        if (!restored_map_view_ && !layers_defaulted_) {
            layers_defaulted_ = true;
            if (ports_layer_btn_ && !ports_layer_btn_->isChecked())
                ports_layer_btn_->setChecked(true);
            if (chokepoints_layer_btn_ && !chokepoints_layer_btn_->isChecked())
                chokepoints_layer_btn_->setChecked(true);
        }
    });
    splitter->addWidget(vessels_pane_);

    vessels_table_->setMinimumHeight(180);
    vessels_pane_->setMinimumHeight(180);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 5);
    splitter->setSizes({380, 220, 360});
    vl->addWidget(splitter, 1);

    return panel;
}

// ── Right Panel ───────────────────────────────────────────────────────────────
QWidget* MaritimeScreen::build_right_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("mtRightPanel"));
    panel->setFixedWidth(std::max(280, mt_text_width(34, -1)));

    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);

    auto* content = new QWidget(scroll);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(10);

    // ── Vessel Search ──────────────────────────────────────────────────────
    search_title_ = new QLabel(tr("VESSEL SEARCH"), content);
    mt_set_role(search_title_, "section");
    vl->addWidget(search_title_);

    imo_cap_ = new QLabel(tr("IMO NUMBER"), content);
    mt_set_role(imo_cap_, "tiny");
    vl->addWidget(imo_cap_);

    imo_edit_ = new QLineEdit(content);
    imo_edit_->setPlaceholderText(tr("e.g. 9344745"));
    connect(imo_edit_, &QLineEdit::returnPressed, this, &MaritimeScreen::on_search_vessel);
    connect(imo_edit_, &QLineEdit::textChanged, this,
            [this](const QString&) { fincept::ScreenStateManager::instance().notify_changed(this); });
    vl->addWidget(imo_edit_);

    track_btn_ = new QPushButton(tr("TRACK"), content);
    track_btn_->setCursor(Qt::PointingHandCursor);
    mt_set_role(track_btn_, "primary");
    connect(track_btn_, &QPushButton::clicked, this, &MaritimeScreen::on_search_vessel);
    vl->addWidget(track_btn_);

    history_btn_ = new QPushButton(tr("VOYAGE HISTORY"), content);
    history_btn_->setCursor(Qt::PointingHandCursor);
    mt_set_role(history_btn_, "outline");
    connect(history_btn_, &QPushButton::clicked, this, [this]() {
        QString imo;
        if (!read_imo(&imo))
            return;
        // A view of the vessel-list pane of its own (5 credits; the service
        // answers a copy younger than 30 min for free).
        MtView v;
        v.imo = imo;
        const QString key = QStringLiteral("history/") + imo;
        views_.insert(key, v);
        configure_vessels_preview(v);
        vessels_pane_->set_cost_credits(5, QStringLiteral("maritime"));
        vessels_pane_->set_view(key);
        vessels_pane_->run();
    });
    vl->addWidget(history_btn_);

    // Selected-vessel card: headline KPIs, the route-progress bar between the
    // two ports, and the fix itself. The card's title is the vessel name.
    search_result_card_ = new ui::Card(tr("SELECTED VESSEL"), content);
    search_result_card_->setVisible(false);
    connect(search_result_card_, &ui::Card::close_requested, search_result_card_, &QWidget::hide);
    auto* srvl = search_result_card_->content_layout();
    srvl->setSpacing(6);

    sr_imo_ = new QLabel(search_result_card_);
    mt_set_role(sr_imo_, "detail");
    srvl->addWidget(sr_imo_);
    sr_kpis_ = new ui::KpiStrip(search_result_card_);
    sr_kpis_->set_max_columns(2);
    sr_kpis_->set_min_tile_width(std::max(100, mt_text_width(9, -2)));
    const std::pair<const char*, QString> sr_tiles[] = {{"speed", tr("SPEED (KN)")},
                                                        {"heading", tr("HEADING")},
                                                        {"draught", tr("DRAUGHT (M)")},
                                                        {"progress", tr("ROUTE PROGRESS")},
                                                        {"age", tr("FIX AGE")}};
    for (const auto& [key, label] : sr_tiles)
        sr_kpis_->add_tile(QString::fromLatin1(key), label)->set_compact(true);
    srvl->addWidget(sr_kpis_);
    sr_progress_ = new ui::RangeBar(search_result_card_);
    sr_progress_->setMinimumHeight(44);
    sr_progress_->set_value_formatter([](double v) { return QStringLiteral("%1%").arg(QLocale().toString(v, 'f', 0)); });
    srvl->addWidget(sr_progress_);
    sr_from_ = new QLabel(search_result_card_);
    mt_set_role(sr_from_, "detail");
    sr_from_->setWordWrap(true);
    srvl->addWidget(sr_from_);
    sr_to_ = new QLabel(search_result_card_);
    mt_set_role(sr_to_, "detail");
    sr_to_->setWordWrap(true);
    srvl->addWidget(sr_to_);
    sr_position_ = new QLabel(search_result_card_);
    mt_set_role(sr_position_, "detail");
    sr_position_->setWordWrap(true);
    srvl->addWidget(sr_position_);
    vl->addWidget(search_result_card_);

    search_result_label_ = new QLabel(content);
    search_result_label_->setObjectName(QStringLiteral("mtSearchError"));
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
    mt_set_role(place_title_, "section");
    vl->addWidget(place_title_);

    place_cap_ = new QLabel(tr("PLACE NAME"), content);
    mt_set_role(place_cap_, "tiny");
    vl->addWidget(place_cap_);

    place_query_edit_ = new QLineEdit(content);
    place_query_edit_->setPlaceholderText(tr("e.g. Strait of Malacca"));
    auto run_place_search = [this]() {
        const QString q = place_query_edit_->text().trimmed();
        if (q.isEmpty())
            return;
        if (place_status_)
            place_status_->setText(tr("Searching…"));
        place_req_ctx_ = q; // GeocodingService echoes the trimmed query as the context
        services::maritime::GeocodingService::instance().search(q);
    };
    connect(place_query_edit_, &QLineEdit::returnPressed, this, run_place_search);

    // Results of an explicit search (Enter / SEARCH PLACES) also feed a completer
    // popup. No search-as-you-type: Nominatim's usage policy forbids client-side
    // autocomplete on its public API (and allows at most one request per second).
    place_completer_model_ = new QStringListModel(this);
    place_completer_ = new QCompleter(place_completer_model_, this);
    place_completer_->setCaseSensitivity(Qt::CaseInsensitive);
    place_completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    place_completer_->setMaxVisibleItems(8);
    place_query_edit_->setCompleter(place_completer_);
    if (auto* popup = place_completer_->popup()) {
        popup->setStyleSheet(maritime_popup_ss());
    }
    // Picking a suggestion loads vessels for that place's bbox.
    connect(place_completer_, QOverload<const QString&>::of(&QCompleter::activated), this, [this](const QString& text) {
        for (const auto& p : place_results_) {
            if (p.display_name == text) {
                run_area_search(p.min_lat, p.max_lat, p.min_lng, p.max_lng);
                return;
            }
        }
    });
    // Kept for hideEvent()'s stop(); never started (see above).
    place_debounce_ = new QTimer(this);
    place_debounce_->setSingleShot(true);
    vl->addWidget(place_query_edit_);

    place_btn_ = new QPushButton(tr("SEARCH PLACES"), content);
    place_btn_->setCursor(Qt::PointingHandCursor);
    mt_set_role(place_btn_, "primary");
    connect(place_btn_, &QPushButton::clicked, this, run_place_search);
    vl->addWidget(place_btn_);

    place_status_ = new QLabel(content);
    mt_set_role(place_status_, "hint");
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
    place_table_->setAccessibleName(tr("Place search results"));
    place_table_->setAccessibleDescription(tr("Click or press Enter on a place to load vessels in its bounding box."));
    // Click (or Enter) a suggestion → fill bbox + load vessels in that area.
    // This was wired to currentCellChanged, which also fires on every arrow-key
    // move — walking the list with the keyboard issued a metered vessel
    // area-search per row.
    auto pick_place = [this](int row, int) {
        if (row < 0 || row >= place_results_.size())
            return;
        const auto& p = place_results_[row];
        run_area_search(p.min_lat, p.max_lat, p.min_lng, p.max_lng);
    };
    connect(place_table_, &QTableWidget::cellClicked, this, pick_place);
    connect(place_table_, &QTableWidget::cellActivated, this, pick_place);
    vl->addWidget(place_table_);
    fill_default_places(); // key shipping areas until the user searches

    // ── Area Search (Advanced — raw lat/long) ───────────────────────────────
    vl->addSpacing(8);
    area_toggle_ = new QPushButton(tr("▸ ADVANCED: RAW BBOX"), content);
    auto* area_toggle = area_toggle_;
    area_toggle->setCursor(Qt::PointingHandCursor);
    area_toggle->setCheckable(true);
    mt_set_role(area_toggle, "outline");
    vl->addWidget(area_toggle);

    advanced_area_box_ = new QWidget(content);
    advanced_area_box_->setVisible(false);
    auto* area_vl = new QVBoxLayout(advanced_area_box_);
    area_vl->setContentsMargins(0, 4, 0, 0);
    area_vl->setSpacing(8);

    auto make_coord = [&](const QString& label, double def_val, QLabel** cap_out) {
        auto* lbl = new QLabel(label, advanced_area_box_);
        mt_set_role(lbl, "tiny");
        area_vl->addWidget(lbl);
        auto* spin = new QDoubleSpinBox(advanced_area_box_);
        spin->setRange(-180, 180);
        spin->setDecimals(4);
        spin->setValue(def_val);
        area_vl->addWidget(spin);
        if (cap_out)
            *cap_out = lbl;
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
    mt_set_role(area_btn_, "primary");
    connect(area_btn_, &QPushButton::clicked, this, [this]() {
        run_area_search(area_min_lat_->value(), area_max_lat_->value(), area_min_lng_->value(), area_max_lng_->value());
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
    mt_set_role(ports_title_, "section");
    vl->addWidget(ports_title_);

    ports_cap_ = new QLabel(tr("PORT NAME"), content);
    mt_set_role(ports_cap_, "tiny");
    vl->addWidget(ports_cap_);

    ports_query_edit_ = new QLineEdit(content);
    ports_query_edit_->setPlaceholderText(tr("e.g. Rotterdam"));
    auto run_name_search = [this]() {
        const QString q = ports_query_edit_->text().trimmed();
        if (q.isEmpty())
            return;
        if (ports_status_)
            ports_status_->setText(tr("Searching…"));
        ports_req_ctx_ = PortsCatalog::name_context(q);
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
        popup->setStyleSheet(maritime_popup_ss());
    }
    // Picking a port suggestion plots it on the map (amber pin) and flies there.
    connect(ports_completer_, QOverload<const QString&>::of(&QCompleter::activated), this, [this](const QString& text) {
        if (!map_widget_)
            return;
        for (const auto& p : port_results_) {
            if (p.name == text) {
                fincept::ui::MapPin pin;
                pin.latitude = p.latitude;
                pin.longitude = p.longitude;
                pin.label = QString("%1 (%2)").arg(p.name, p.locode.isEmpty() ? p.country : p.locode);
                pin.color = mt_marker_color(&ui::ThemeTokens::accent);
                pin.radius = 6.0;
                pin.id = -1;
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
        if (q.size() < 3)
            return;
        if (ports_status_)
            ports_status_->setText(tr("Searching…"));
        ports_req_ctx_ = PortsCatalog::name_context(q);
        PortsCatalog::instance().search_by_name(q);
    });
    connect(ports_query_edit_, &QLineEdit::textEdited, this, [this](const QString&) { ports_debounce_->start(); });
    vl->addWidget(ports_query_edit_);

    ports_btn_ = new QPushButton(tr("SEARCH PORTS"), content);
    ports_btn_->setCursor(Qt::PointingHandCursor);
    mt_set_role(ports_btn_, "primary");
    connect(ports_btn_, &QPushButton::clicked, this, run_name_search);
    vl->addWidget(ports_btn_);

    ports_in_view_btn_ = new QPushButton(tr("PORTS IN AREA"), content);
    ports_in_view_btn_->setCursor(Qt::PointingHandCursor);
    mt_set_role(ports_in_view_btn_, "outline");
    connect(ports_in_view_btn_, &QPushButton::clicked, this, [this]() {
        if (!area_min_lat_ || !area_max_lat_ || !area_min_lng_ || !area_max_lng_)
            return;
        const double mn_lat = area_min_lat_->value();
        const double mx_lat = area_max_lat_->value();
        const double mn_lng = area_min_lng_->value();
        const double mx_lng = area_max_lng_->value();
        if (mx_lat <= mn_lat || mx_lng <= mn_lng) {
            if (ports_status_)
                ports_status_->setText(tr("Set a valid bbox in AREA SEARCH first."));
            return;
        }
        if (ports_status_)
            ports_status_->setText(tr("Searching…"));
        ports_req_ctx_ = PortsCatalog::bbox_context(mn_lat, mx_lat, mn_lng, mx_lng);
        PortsCatalog::instance().search_by_bbox(mn_lat, mx_lat, mn_lng, mx_lng);
    });
    vl->addWidget(ports_in_view_btn_);

    ports_status_ = new QLabel(content);
    mt_set_role(ports_status_, "hint");
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
    ports_table_->setAccessibleName(tr("Port search results"));
    ports_table_->setAccessibleDescription(tr("Click or press Enter on a port to plot it on the map."));
    // Click (or Enter) → drop an AMBER pin on the existing world map to
    // distinguish from the green vessel pins, then re-fit the camera. Was on
    // currentCellChanged, so arrow-keying the list flew the map on every row.
    auto pick_port = [this](int row, int) {
        if (row < 0 || row >= port_results_.size() || !map_widget_)
            return;
        const auto& p = port_results_[row];
        // Add port pin on top of existing vessel pins; id=-1 keeps the port
        // pin out of the pin_clicked dispatch (which expects vessel indices).
        fincept::ui::MapPin pin;
        pin.latitude = p.latitude;
        pin.longitude = p.longitude;
        pin.label = QString("%1 (%2)").arg(p.name, p.locode.isEmpty() ? p.country : p.locode);
        pin.color = mt_marker_color(&ui::ThemeTokens::accent);
        pin.radius = 6.0;
        pin.id = -1;
        map_widget_->add_pin(pin);
        map_widget_->fly_to(p.latitude, p.longitude);
    };
    connect(ports_table_, &QTableWidget::cellClicked, this, pick_port);
    connect(ports_table_, &QTableWidget::cellActivated, this, pick_port);
    vl->addWidget(ports_table_);
    fill_default_ports(); // the busiest container ports until the user searches

    // ── Route Detail ───────────────────────────────────────────────────────
    vl->addSpacing(8);
    route_detail_ = new QWidget(content);
    route_detail_->setObjectName(QStringLiteral("mtRouteCard"));
    route_detail_->setVisible(false);
    auto* rdvl = new QVBoxLayout(route_detail_);
    rdvl->setContentsMargins(10, 10, 10, 10);
    rdvl->setSpacing(4);

    rd_title_ = new QLabel(tr("SELECTED ROUTE"), route_detail_);
    mt_set_role(rd_title_, "cardCaption");
    rdvl->addWidget(rd_title_);

    rd_name_ = new QLabel(route_detail_);
    mt_set_role(rd_name_, "cardTitle");
    rdvl->addWidget(rd_name_);
    rd_value_ = new QLabel(route_detail_);
    mt_set_role(rd_value_, "detail");
    rdvl->addWidget(rd_value_);
    rd_status_ = new QLabel(route_detail_);
    mt_set_role(rd_status_, "detail");
    rdvl->addWidget(rd_status_);
    rd_vessels_ = new QLabel(route_detail_);
    mt_set_role(rd_vessels_, "detail");
    rdvl->addWidget(rd_vessels_);
    rd_clear_btn_ = new QPushButton(tr("SHOW ALL VESSELS"), route_detail_);
    rd_clear_btn_->setCursor(Qt::PointingHandCursor);
    rd_clear_btn_->setToolTip(tr("Drop the corridor filter and show the whole loaded fleet again"));
    mt_set_role(rd_clear_btn_, "outline");
    connect(rd_clear_btn_, &QPushButton::clicked, this, &MaritimeScreen::clear_route_filter);
    rdvl->addWidget(rd_clear_btn_);
    vl->addWidget(route_detail_);

    // ── System Status ──────────────────────────────────────────────────────
    vl->addSpacing(8);
    not_found_label_ = new QLabel(content);
    not_found_label_->setVisible(false);
    not_found_label_->setWordWrap(true);
    not_found_label_->setObjectName(QStringLiteral("mtNotFound"));
    vl->addWidget(not_found_label_);

    // Suppressed: hardcoded "system status" placeholders (AIS Transponders /
    // Satellite Imagery / 10 corridors / 13 SATs / 6 ports) — none of these
    // figures come from the API. The empty vector + dead loop that replaced
    // them has been removed; restore a real block once a backend exposes it.


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
    bar->setObjectName(QStringLiteral("mtStatusBar"));
    bar->setMinimumHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(16);

    sb_source_cap_ = new QLabel(tr("SOURCE:"), bar);
    mt_set_role(sb_source_cap_, "sbCap");
    source_value_ = new QLabel("—", bar);
    mt_set_role(source_value_, "sbVal");
    hl->addWidget(sb_source_cap_);
    hl->addWidget(source_value_);

    sb_records_cap_ = new QLabel(tr("RECORDS:"), bar);
    mt_set_role(sb_records_cap_, "sbCap");
    records_value_ = new QLabel("—", bar);
    mt_set_role(records_value_, "sbVal");
    hl->addWidget(sb_records_cap_);
    hl->addWidget(records_value_);

    // Real last-update clock (set on each successful load) — replaces the old
    // hardcoded "5 MIN" refresh label.
    sb_refresh_cap_ = new QLabel(tr("UPDATED:"), bar);
    mt_set_role(sb_refresh_cap_, "sbCap");
    sb_refresh_val_ = new QLabel(QStringLiteral("—"), bar);
    mt_set_role(sb_refresh_val_, "sbVal");
    hl->addWidget(sb_refresh_cap_);
    hl->addWidget(sb_refresh_val_);

    hl->addStretch();

    status_label_ = new QLabel(tr("READY"), bar);
    status_label_->setObjectName(QStringLiteral("mtStatus"));
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
        auto* count_item = new ui::NumericTableWidgetItem(QLocale().toString(r.vessels), r.vessels);
        count_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        routes_table_->setItem(i, 1, count_item);
    }
    routes_table_->resizeColumnsToContents();
}

// ── Built-in starting points (free, no request) ──────────────────────────────

namespace {

/// Key shipping areas: chokepoints and lanes most users look up first. The
/// bounding boxes are deliberately tight so an area search stays focused.
struct MtLayArea {
    const char* name;
    const char* type;
    double min_lat, max_lat, min_lng, max_lng;
};
constexpr MtLayArea kMtLayAreas[] = {
    {"Strait of Malacca", "strait", 1.0, 6.5, 98.0, 104.5},
    {"Singapore Strait", "strait", 1.0, 1.5, 103.4, 104.5},
    {"Strait of Hormuz", "strait", 25.5, 27.2, 55.5, 57.5},
    {"Suez Canal", "canal", 29.8, 31.4, 32.2, 32.7},
    {"Bab-el-Mandeb", "strait", 12.2, 13.2, 42.9, 43.8},
    {"Panama Canal", "canal", 8.8, 9.5, -80.0, -79.4},
    {"Strait of Dover", "strait", 50.6, 51.3, 0.9, 2.0},
    {"Strait of Gibraltar", "strait", 35.8, 36.2, -6.1, -5.2},
    {"Bosphorus", "strait", 40.95, 41.25, 28.9, 29.2},
    {"Taiwan Strait", "strait", 22.5, 25.5, 118.5, 121.0},
    {"Cape of Good Hope", "cape", -35.5, -33.5, 17.5, 20.5},
    {"Rotterdam approaches", "port area", 51.85, 52.05, 3.9, 4.5},
};

/// The busiest container ports (TEU), for the ports table until a search.
struct MtLayPort {
    const char* name;
    const char* country;
    const char* locode;
    double lat, lng;
};
constexpr MtLayPort kMtLayPorts[] = {
    {"Shanghai", "China", "CNSHA", 31.23, 121.49},       {"Singapore", "Singapore", "SGSIN", 1.26, 103.84},
    {"Ningbo-Zhoushan", "China", "CNNGB", 29.87, 121.85}, {"Shenzhen", "China", "CNSZX", 22.50, 113.88},
    {"Qingdao", "China", "CNTAO", 36.08, 120.32},        {"Guangzhou", "China", "CNCAN", 22.75, 113.62},
    {"Busan", "South Korea", "KRPUS", 35.10, 129.04},    {"Tianjin", "China", "CNTXG", 38.97, 117.75},
    {"Hong Kong", "China", "HKHKG", 22.30, 114.17},       {"Rotterdam", "Netherlands", "NLRTM", 51.95, 4.14},
    {"Port Klang", "Malaysia", "MYPKG", 3.00, 101.39},    {"Jebel Ali", "UAE", "AEJEA", 25.01, 55.06},
    {"Antwerp-Bruges", "Belgium", "BEANR", 51.26, 4.40},  {"Xiamen", "China", "CNXMN", 24.48, 118.08},
    {"Tanjung Pelepas", "Malaysia", "MYTPP", 1.36, 103.55}, {"Los Angeles", "United States", "USLAX", 33.74, -118.26},
    {"Long Beach", "United States", "USLGB", 33.75, -118.20}, {"Kaohsiung", "Taiwan", "TWKHH", 22.61, 120.29},
    {"Laem Chabang", "Thailand", "THLCH", 13.08, 100.88}, {"Hamburg", "Germany", "DEHAM", 53.54, 9.97},
};

} // namespace

void MaritimeScreen::fill_default_places() {
    if (!place_table_)
        return;
    place_results_.clear();
    for (const auto& a : kMtLayAreas) {
        GeoPlace p;
        p.display_name = QString::fromUtf8(a.name);
        p.type = QString::fromUtf8(a.type);
        p.min_lat = a.min_lat;
        p.max_lat = a.max_lat;
        p.min_lng = a.min_lng;
        p.max_lng = a.max_lng;
        p.latitude = (a.min_lat + a.max_lat) / 2.0;
        p.longitude = (a.min_lng + a.max_lng) / 2.0;
        place_results_.append(p);
    }
    place_table_->setSortingEnabled(false);
    place_table_->setRowCount(place_results_.size());
    for (int i = 0; i < place_results_.size(); ++i) {
        const auto& p = place_results_[i];
        auto* name = new QTableWidgetItem(p.display_name);
        name->setToolTip(tr("Load the vessels in %1 (8 credits)").arg(p.display_name));
        place_table_->setItem(i, 0, name);
        place_table_->setItem(i, 1, new QTableWidgetItem(p.type));
    }
    if (place_status_)
        place_status_->setText(tr("Key shipping lanes — click one to load its vessels (8 credits)."));
}

void MaritimeScreen::fill_default_ports() {
    if (!ports_table_)
        return;
    port_results_.clear();
    for (const auto& b : kMtLayPorts) {
        PortRecord r;
        r.name = QString::fromUtf8(b.name);
        r.country = QString::fromUtf8(b.country);
        r.locode = QString::fromLatin1(b.locode);
        r.latitude = b.lat;
        r.longitude = b.lng;
        port_results_.append(r);
    }
    ports_table_->setSortingEnabled(false);
    ports_table_->setRowCount(port_results_.size());
    for (int i = 0; i < port_results_.size(); ++i) {
        const auto& p = port_results_[i];
        auto* name = new QTableWidgetItem(p.name);
        name->setToolTip(QString("%1\nUN/LOCODE: %2").arg(p.name, p.locode));
        ports_table_->setItem(i, 0, name);
        ports_table_->setItem(i, 1, new QTableWidgetItem(p.country));
        ports_table_->setItem(i, 2, new QTableWidgetItem(tr("Top 20")));
    }
    if (ports_status_)
        ports_status_->setText(tr("The 20 busiest container ports — click one to plot it (free)."));
}
} // namespace fincept::screens
