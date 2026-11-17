// src/screens/surface_analytics/SurfaceAnalyticsScreen_Layout.cpp
//
// UI construction for the volatility/surface analytics screen: setup_ui,
// build_category_bar, build_surface_bar, refresh_surface_bar, and the
// view-mode button skinning (apply_view_mode_buttons). The category-accent
// colour helper lives here too — only the layout code consumes it.
//
// Part of the partial-class split of SurfaceAnalyticsScreen.cpp.

#include "Surface3DWidget.h"
#include "SurfaceAnalyticsScreen.h"
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
#include "ui/widgets/EmptyState.h"

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>
#include <QVariant>

namespace fincept::surface {

using namespace fincept::ui;

// ── Helpers ──────────────────────────────────────────────────────────────────
// Category accent: the theme's categorical chart colour for the category's
// position (EQUITY DERIV, FIXED INCOME, FX, CREDIT, COMMODITIES, RISK, MACRO).
static QString cat_hex(int i) {
    return colors::chart_color(i).name();
}

static QLabel* make_sep(QWidget* parent) {
    auto* s = new QLabel("|", parent);
    s->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; background:transparent;"
                             " font-family:%2;")
                         .arg(colors::BORDER_MED())
                         .arg(fonts::DATA_FAMILY()));
    return s;
}

static QString btn_inactive() {
    return QString("QPushButton { background:%1; border:1px solid %2; color:%3;"
                   " font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:bold; font-family:%4;"
                   " padding:0 10px; }"
                   "QPushButton:hover { background:%5; color:%6; border-color:%7; }")
        .arg(colors::BG_RAISED())
        .arg(colors::BORDER_DIM())
        .arg(colors::TEXT_SECONDARY())
        .arg(fonts::DATA_FAMILY())
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
                      .arg(fonts::DATA_FAMILY()));

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

    demo_banner_ = new QLabel(tr("SAMPLE DATA — synthetic values, not a live market surface. "
                                 "Do not trade or quote from this."),
                              right);
    demo_banner_->setWordWrap(true);
    demo_banner_->setAlignment(Qt::AlignCenter);
    demo_banner_->setAccessibleName(tr("Synthetic data warning"));
    // One stylesheet, set once: the banner's tone is a dynamic property ("sample" amber,
    // "delayed" info, "loading" neutral) — update_source_banner() only re-polishes.
    demo_banner_->setProperty("tone", QStringLiteral("sample"));
    demo_banner_->setStyleSheet(QString("QLabel { background:%3; color:%1; border-bottom:1px solid %2;"
                                        " font-size:%4px; font-weight:bold; letter-spacing:0.4px; padding:5px 8px; }"
                                        "QLabel[tone=\"delayed\"] { background:%5; color:%6; border-bottom:1px solid %7; }"
                                        "QLabel[tone=\"loading\"] { background:%8; color:%9; border-bottom:1px solid %10; }")
                                    .arg(colors::AMBER())
                                    .arg(colors::AMBER_DIM())
                                    .arg(colors::alpha(colors::AMBER, 0.18))
                                    .arg(fincept::ui::fonts::px(-4))
                                    .arg(colors::alpha(colors::INFO, 0.14))
                                    .arg(colors::INFO())
                                    .arg(colors::alpha(colors::INFO, 0.45))
                                    .arg(colors::BG_RAISED())
                                    .arg(colors::TEXT_SECONDARY())
                                    .arg(colors::BORDER_DIM()));
    rvl->addWidget(demo_banner_);

    view_stack_ = new QStackedWidget(right);
    surface_3d_ = new Surface3DWidget(view_stack_);
    surface_table_ = new SurfaceTableWidget(view_stack_);
    surface_line_ = new SurfaceLineWidget(view_stack_);
    view_stack_->addWidget(surface_3d_);    // 0
    view_stack_->addWidget(surface_table_); // 1
    view_stack_->addWidget(surface_line_);  // 2
    // 3: the option surfaces' page when the free delayed feed has no chain for the
    // underlying (show_free_failure_page) — BACK TO the last one that loaded, RETRY.
    chain_state_ = new fincept::ui::EmptyState(view_stack_);
    view_stack_->addWidget(chain_state_);
    connect(chain_state_, &fincept::ui::EmptyState::primary_clicked, this, [this]() {
        if (!chain_state_back_.isEmpty() && control_panel_)
            control_panel_->choose_symbol(chain_state_back_);
        else
            start_free_chain(true);
    });
    connect(chain_state_, &fincept::ui::EmptyState::secondary_clicked, this, [this]() { start_free_chain(true); });
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
    connect(control_panel_, &SurfaceControlPanel::controls_changed, this, &SurfaceAnalyticsScreen::on_controls_changed);
    connect(control_panel_, &SurfaceControlPanel::symbol_changed, this,
            &SurfaceAnalyticsScreen::on_control_symbol_changed);
    connect(control_panel_, &SurfaceControlPanel::fetch_requested, this, &SurfaceAnalyticsScreen::on_fetch_requested);

    // Default visibility / capability for active surface
    control_panel_->set_capability(active_chart_);
}

// ── Category bar (32px, Obsidian tab style) ──────────────────────────────────
QWidget* SurfaceAnalyticsScreen::build_category_bar() {
    auto* bar = new QWidget(this);
    bar->setMinimumHeight(32);
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
        btn->setMinimumHeight(32);

        if (active) {
            btn->setStyleSheet(QString("QPushButton { background:%4; color:%1;"
                                       " border:none; border-bottom:2px solid %2;"
                                       " padding:0 14px; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:bold;"
                                       " font-family:%3; }"
                                       "QPushButton:hover { background:%4; }")
                                   .arg(colors::TEXT_ON_ACCENT())
                                   .arg(cat_hex(i))
                                   .arg(fonts::DATA_FAMILY())
                                   .arg(colors::AMBER()));
        } else {
            btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1;"
                                       " border:none; padding:0 14px; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                       " font-family:%2; }"
                                       "QPushButton:hover { background:%3; color:%4; }")
                                   .arg(colors::TEXT_SECONDARY())
                                   .arg(fonts::DATA_FAMILY())
                                   .arg(colors::BG_RAISED())
                                   .arg(colors::TEXT_PRIMARY()));
        }

        btn->setProperty("cat_index", i);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_category_clicked(i); });
        hl->addWidget(btn);
    }

    hl->addStretch();

    // Right controls — flat Obsidian buttons
    import_btn_ = new QPushButton(tr("IMPORT CSV"), bar);
    import_btn_->setMinimumHeight(20);
    import_btn_->setStyleSheet(btn_inactive());
    connect(import_btn_, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_import_csv);
    hl->addWidget(import_btn_);

    hl->addSpacing(4);
    hl->addWidget(make_sep(bar));
    hl->addSpacing(4);

    btn_3d_ = new QPushButton(tr("3D"), bar);
    btn_table_ = new QPushButton(tr("TABLE"), bar);
    btn_line_ = new QPushButton(tr("LINE"), bar);
    btn_3d_->setMinimumHeight(20);
    btn_table_->setMinimumHeight(20);
    btn_line_->setMinimumHeight(20);
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

    refresh_btn_ = new QPushButton(tr("REFRESH"), bar);
    refresh_btn_->setProperty("fincept_agent_risk", "spend"); // re-fetches from Databento when a key is set
    refresh_btn_->setMinimumHeight(20);
    refresh_btn_->setStyleSheet(btn_inactive());
    connect(refresh_btn_, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_refresh);
    hl->addWidget(refresh_btn_);

    return bar;
}

// ── Surface chip bar (26px, hairline) ────────────────────────────────────────
QWidget* SurfaceAnalyticsScreen::build_surface_bar() {
    auto* bar = new QWidget(this);
    bar->setMinimumHeight(26);
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
    cat_lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:bold; background:transparent;"
                                   " font-family:%2;")
                               .arg(acol)
                               .arg(fonts::DATA_FAMILY()));
    hl->addWidget(cat_lbl);
    hl->addWidget(make_sep(bar));

    for (int i = 0; i < (int)cat.types.size(); i++) {
        bool active = (cat.types[i] == active_chart_);
        const char* name = chart_type_name(cat.types[i]);

        auto* btn = new QPushButton(name, bar);
        btn->setMinimumHeight(18);

        if (active) {
            btn->setStyleSheet(QString("QPushButton { background:%5; border:1px solid %1; color:%2;"
                                       " font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:bold; font-family:%3; padding:0 8px; }"
                                       "QPushButton:hover { background:%2; color:%4; }")
                                   .arg(colors::AMBER_DIM())
                                   .arg(colors::AMBER())
                                   .arg(fonts::DATA_FAMILY())
                                   .arg(colors::TEXT_ON_ACCENT())
                                   .arg(colors::alpha(colors::AMBER, 0.12)));
        } else {
            btn->setStyleSheet(QString("QPushButton { background:transparent; border:none; color:%1;"
                                       " font-size:" + fincept::ui::fonts::px(-3) + "px; font-family:%2; padding:0 8px; }"
                                       "QPushButton:hover { color:%3; border-bottom:1px solid %4; }")
                                   .arg(colors::TEXT_SECONDARY())
                                   .arg(fonts::DATA_FAMILY())
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
        return QString("QPushButton { background:%4; color:%1; "
                       "border:1px solid %2; padding:0 10px; font-size:" + fincept::ui::fonts::px(-3) + "px; "
                       "font-weight:bold; font-family:%3; }")
            .arg(colors::AMBER())
            .arg(colors::AMBER_DIM())
            .arg(fonts::DATA_FAMILY())
            .arg(colors::alpha(colors::AMBER, 0.18));
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
