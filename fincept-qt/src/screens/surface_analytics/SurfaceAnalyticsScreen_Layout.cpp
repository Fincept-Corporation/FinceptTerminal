// src/screens/surface_analytics/SurfaceAnalyticsScreen_Layout.cpp
//
// UI construction for the volatility/surface analytics screen: setup_ui,
// build_category_bar, build_surface_bar, refresh_surface_bar, and the
// view-mode button skinning (apply_view_mode_buttons). The category-accent
// colour helpers and the MONO font constant live here too — only the layout
// code consumes them.
//
// Part of the partial-class split of SurfaceAnalyticsScreen.cpp.

#include "SurfaceAnalyticsScreen.h"

#include "Surface3DWidget.h"
#include "SurfaceCapabilities.h"
#include "SurfaceControlPanel.h"
#include "SurfaceCsvImporter.h"
#include "SurfaceDataInspector.h"
#include "SurfaceDefaults.h"
#include "SurfaceLineWidget.h"
#include "SurfaceTableWidget.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QVariant>
#include <QVBoxLayout>

namespace fincept::surface {

using namespace fincept::ui;

static const char* MONO = "'Consolas','Courier New',monospace";

// ── Category accent colors (muted, functional only) ─────────────────────────
// R,G,B kept but used as muted accent — no bright blobs
static constexpr int CAT_ACCENT[][3] = {
    {88, 166, 255},  // EQUITY DERIV
    {63, 185, 80},   // FIXED INCOME
    {217, 164, 6},   // FX
    {220, 80, 80},   // CREDIT
    {155, 114, 255}, // COMMODITIES
    {217, 119, 6},   // RISK  → amber
    {89, 196, 217},  // MACRO
};

// ── Helpers ──────────────────────────────────────────────────────────────────
static QString cat_hex(int i) {
    return QString("rgb(%1,%2,%3)").arg(CAT_ACCENT[i][0]).arg(CAT_ACCENT[i][1]).arg(CAT_ACCENT[i][2]);
}

static QLabel* make_sep(QWidget* parent) {
    auto* s = new QLabel("|", parent);
    s->setStyleSheet(QString("color:%1; font-size:11px; background:transparent;"
                             " font-family:%2;")
                         .arg(colors::BORDER_MED())
                         .arg(MONO));
    return s;
}

static QString btn_inactive() {
    return QString("QPushButton { background:%1; border:1px solid %2; color:%3;"
                   " font-size:11px; font-weight:bold; font-family:%4;"
                   " padding:0 10px; }"
                   "QPushButton:hover { background:%5; color:%6; border-color:%7; }")
        .arg(colors::BG_RAISED())
        .arg(colors::BORDER_DIM())
        .arg(colors::TEXT_SECONDARY())
        .arg(MONO)
        .arg(colors::BG_HOVER())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BORDER_BRIGHT());
}

// ── Constructor ──────────────────────────────────────────────────────────────
SurfaceAnalyticsScreen::SurfaceAnalyticsScreen(QWidget* parent) : QWidget(parent) {
    srand((unsigned)time(nullptr));
    setup_ui();
    // Default to the seeded equity underlyings on first open so demo data fills.
    if (!control_panel_->state().basket.isEmpty())
        control_panel_->set_capability(active_chart_);
    load_demo_data();
    update_chart();
    update_metrics();
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::setup_ui() {
    setStyleSheet(QString("QWidget { background:%1; color:%2; font-family:%3; }")
                      .arg(colors::BG_BASE())
                      .arg(colors::TEXT_PRIMARY())
                      .arg(MONO));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Row 1 — category bar
    category_bar_ = build_category_bar();
    root->addWidget(category_bar_);

    // Row 2 — surface chip bar
    surface_bar_ = build_surface_bar();
    root->addWidget(surface_bar_);

    // Vertical split: top = horizontal split (control panel | view stack), bottom = data inspector
    auto* outer = new QSplitter(Qt::Vertical, this);
    outer->setHandleWidth(1);
    outer->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(colors::BORDER_DIM()));

    auto* hsplit = new QSplitter(Qt::Horizontal, outer);
    hsplit->setHandleWidth(1);
    hsplit->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(colors::BORDER_DIM()));

    control_panel_ = new SurfaceControlPanel(hsplit);
    hsplit->addWidget(control_panel_);

    auto* right = new QWidget(hsplit);
    right->setStyleSheet(QString("background:%1;").arg(colors::BG_BASE()));
    auto* rvl = new QVBoxLayout(right);
    rvl->setContentsMargins(0, 0, 0, 0);
    rvl->setSpacing(0);

    view_stack_ = new QStackedWidget(right);
    surface_3d_ = new Surface3DWidget(view_stack_);
    surface_table_ = new SurfaceTableWidget(view_stack_);
    surface_line_ = new SurfaceLineWidget(view_stack_);
    view_stack_->addWidget(surface_3d_);    // 0
    view_stack_->addWidget(surface_table_); // 1
    view_stack_->addWidget(surface_line_);  // 2
    rvl->addWidget(view_stack_, 1);

    hsplit->addWidget(right);
    hsplit->setStretchFactor(0, 0);
    hsplit->setStretchFactor(1, 1);
    outer->addWidget(hsplit);

    data_inspector_ = new SurfaceDataInspector(outer);
    outer->addWidget(data_inspector_);
    outer->setStretchFactor(0, 1);
    outer->setStretchFactor(1, 0);
    outer->setSizes({600, 220});

    root->addWidget(outer, 1);

    // Wire control panel signals
    connect(control_panel_, &SurfaceControlPanel::controls_changed, this,
            &SurfaceAnalyticsScreen::on_controls_changed);
    connect(control_panel_, &SurfaceControlPanel::symbol_changed, this,
            &SurfaceAnalyticsScreen::on_control_symbol_changed);
    connect(control_panel_, &SurfaceControlPanel::fetch_requested, this,
            &SurfaceAnalyticsScreen::on_fetch_requested);

    // Default visibility / capability for active surface
    control_panel_->set_capability(active_chart_);
}

// ── Category bar (32px, Obsidian tab style) ──────────────────────────────────
QWidget* SurfaceAnalyticsScreen::build_category_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(32);
    bar->setStyleSheet(QString("QWidget { background:%1; border-bottom:1px solid %2; }")
                           .arg(colors::BG_SURFACE())
                           .arg(colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(0);

    const auto categories = get_surface_categories();
    for (int i = 0; i < (int)categories.size(); i++) {
        bool active = (i == active_category_);
        auto* btn = new QPushButton(categories[i].name, bar);
        btn->setFixedHeight(32);

        if (active) {
            btn->setStyleSheet(QString("QPushButton { background:#b45309; color:%1;"
                                       " border:none; border-bottom:2px solid %2;"
                                       " padding:0 14px; font-size:11px; font-weight:bold;"
                                       " font-family:%3; }"
                                       "QPushButton:hover { background:#b45309; }")
                                   .arg(colors::TEXT_PRIMARY())
                                   .arg(cat_hex(i))
                                   .arg(MONO));
        } else {
            btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1;"
                                       " border:none; padding:0 14px; font-size:11px;"
                                       " font-family:%2; }"
                                       "QPushButton:hover { background:%3; color:%4; }")
                                   .arg(colors::TEXT_TERTIARY())
                                   .arg(MONO)
                                   .arg(colors::BG_RAISED())
                                   .arg(colors::TEXT_SECONDARY()));
        }

        btn->setProperty("cat_index", i);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_category_clicked(i); });
        hl->addWidget(btn);
    }

    hl->addStretch();

    // Right controls — flat Obsidian buttons
    auto* import_btn = new QPushButton("IMPORT CSV", bar);
    import_btn->setFixedHeight(20);
    import_btn->setStyleSheet(btn_inactive());
    connect(import_btn, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_import_csv);
    hl->addWidget(import_btn);

    hl->addSpacing(4);
    hl->addWidget(make_sep(bar));
    hl->addSpacing(4);

    btn_3d_ = new QPushButton("3D", bar);
    btn_table_ = new QPushButton("TABLE", bar);
    btn_line_ = new QPushButton("LINE", bar);
    btn_3d_->setFixedHeight(20);
    btn_table_->setFixedHeight(20);
    btn_line_->setFixedHeight(20);
    btn_3d_->setCheckable(true);
    btn_table_->setCheckable(true);
    btn_line_->setCheckable(true);
    btn_3d_->setChecked(view_mode_ == ViewMode::Surface3D);
    btn_table_->setChecked(view_mode_ == ViewMode::Table);
    btn_line_->setChecked(view_mode_ == ViewMode::Line);

    apply_view_mode_buttons();

    connect(btn_3d_, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_view_3d);
    connect(btn_table_, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_view_table);
    connect(btn_line_, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_view_line);
    hl->addWidget(btn_3d_);
    hl->addWidget(btn_table_);
    hl->addWidget(btn_line_);

    hl->addSpacing(4);
    hl->addWidget(make_sep(bar));
    hl->addSpacing(4);

    auto* ref_btn = new QPushButton("REFRESH", bar);
    ref_btn->setFixedHeight(20);
    ref_btn->setStyleSheet(btn_inactive());
    connect(ref_btn, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_refresh);
    hl->addWidget(ref_btn);

    return bar;
}

// ── Surface chip bar (26px, hairline) ────────────────────────────────────────
QWidget* SurfaceAnalyticsScreen::build_surface_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(26);
    bar->setStyleSheet(QString("QWidget { background:%1; border-bottom:1px solid %2; }")
                           .arg(colors::BG_SURFACE())
                           .arg(colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(4);

    const auto categories = get_surface_categories();
    if (active_category_ >= (int)categories.size())
        return bar;

    const auto& cat = categories[active_category_];
    const QString acol = cat_hex(active_category_);

    // Category label prefix
    auto* cat_lbl = new QLabel(QString("■ %1").arg(categories[active_category_].name), bar);
    cat_lbl->setStyleSheet(QString("color:%1; font-size:11px; font-weight:bold; background:transparent;"
                                   " font-family:%2;")
                               .arg(acol)
                               .arg(MONO));
    hl->addWidget(cat_lbl);
    hl->addWidget(make_sep(bar));

    for (int i = 0; i < (int)cat.types.size(); i++) {
        bool active = (cat.types[i] == active_chart_);
        const char* name = chart_type_name(cat.types[i]);

        auto* btn = new QPushButton(name, bar);
        btn->setFixedHeight(18);

        if (active) {
            btn->setStyleSheet(QString("QPushButton { background:rgba(217,119,6,0.12); border:1px solid %1; color:%2;"
                                       " font-size:11px; font-weight:bold; font-family:%3; padding:0 8px; }"
                                       "QPushButton:hover { background:%2; color:%4; }")
                                   .arg(colors::AMBER_DIM())
                                   .arg(colors::AMBER())
                                   .arg(MONO)
                                   .arg(colors::BG_BASE()));
        } else {
            btn->setStyleSheet(QString("QPushButton { background:transparent; border:none; color:%1;"
                                       " font-size:11px; font-family:%2; padding:0 8px; }"
                                       "QPushButton:hover { color:%3; border-bottom:1px solid %4; }")
                                   .arg(colors::TEXT_SECONDARY())
                                   .arg(MONO)
                                   .arg(colors::TEXT_PRIMARY())
                                   .arg(acol));
        }

        connect(btn, &QPushButton::clicked, this, [this, i]() { on_surface_clicked(active_category_, i); });
        hl->addWidget(btn);
    }

    hl->addStretch();
    return bar;
}


void SurfaceAnalyticsScreen::apply_view_mode_buttons() {
    auto active = [&]() {
        return QString("QPushButton { background:rgba(217,119,6,0.18); color:%1; "
                       "border:1px solid %2; padding:0 10px; font-size:11px; "
                       "font-weight:bold; font-family:%3; }")
            .arg(colors::AMBER())
            .arg(colors::AMBER_DIM())
            .arg(MONO);
    };
    if (btn_3d_) {
        btn_3d_->setChecked(view_mode_ == ViewMode::Surface3D);
        btn_3d_->setStyleSheet(view_mode_ == ViewMode::Surface3D ? active() : btn_inactive());
    }
    if (btn_table_) {
        btn_table_->setChecked(view_mode_ == ViewMode::Table);
        btn_table_->setStyleSheet(view_mode_ == ViewMode::Table ? active() : btn_inactive());
    }
    if (btn_line_) {
        btn_line_->setChecked(view_mode_ == ViewMode::Line);
        btn_line_->setStyleSheet(view_mode_ == ViewMode::Line ? active() : btn_inactive());
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────
} // namespace fincept::surface
