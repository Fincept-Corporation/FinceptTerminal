#include "SurfaceAnalyticsScreen.h"

#include "Surface3DWidget.h"
#include "SurfaceCsvImporter.h"
#include "SurfaceDatabentoPanel.h"
#include "SurfaceMetricsPanel.h"
#include "SurfaceTableWidget.h"
#include "core/session/ScreenStateManager.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <cstdlib>
#include <ctime>

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

static QString btn_active_amber() {
    return QString("QPushButton { background:rgba(217,119,6,0.12); border:1px solid %1; color:%2;"
                   " font-size:11px; font-weight:bold; font-family:%3;"
                   " padding:0 10px; }"
                   "QPushButton:hover { background:%2; color:%4; }")
        .arg(colors::AMBER_DIM())
        .arg(colors::AMBER())
        .arg(MONO)
        .arg(colors::BG_BASE());
}

// ── Constructor ──────────────────────────────────────────────────────────────
SurfaceAnalyticsScreen::SurfaceAnalyticsScreen(QWidget* parent) : QWidget(parent) {
    srand((unsigned)time(nullptr));
    setup_ui();
    load_demo_data();
    update_chart();
    update_metrics();
}

// ── Layout ───────────────────────────────────────────────────────────────────
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

    // Content — metrics panel | chart area
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(colors::BORDER_DIM()));

    metrics_panel_ = new SurfaceMetricsPanel(splitter);
    splitter->addWidget(metrics_panel_);

    // Right: view stack + databento panel
    auto* right = new QWidget(splitter);
    right->setStyleSheet(QString("background:%1;").arg(colors::BG_BASE()));
    auto* rvl = new QVBoxLayout(right);
    rvl->setContentsMargins(0, 0, 0, 0);
    rvl->setSpacing(0);

    view_stack_ = new QStackedWidget(right);
    surface_3d_ = new Surface3DWidget(view_stack_);
    surface_table_ = new SurfaceTableWidget(view_stack_);
    view_stack_->addWidget(surface_3d_);    // 0
    view_stack_->addWidget(surface_table_); // 1
    rvl->addWidget(view_stack_, 1);

    databento_panel_ = new SurfaceDatabentoPanel(right);
    databento_panel_->setFixedHeight(200);
    rvl->addWidget(databento_panel_);

    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    root->addWidget(splitter, 1);
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
    btn_3d_->setFixedHeight(20);
    btn_table_->setFixedHeight(20);
    btn_3d_->setCheckable(true);
    btn_table_->setCheckable(true);
    btn_3d_->setChecked(true);

    auto tab_style = [&](bool checked) { return checked ? btn_active_amber() : btn_inactive(); };
    btn_3d_->setStyleSheet(tab_style(true));
    btn_table_->setStyleSheet(tab_style(false));

    connect(btn_3d_, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_view_3d);
    connect(btn_table_, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_view_table);
    hl->addWidget(btn_3d_);
    hl->addWidget(btn_table_);

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

    // Symbol selector — equity derivatives only
    if (active_category_ == 0) {
        auto* sym_lbl = new QLabel("SYM:", bar);
        sym_lbl->setStyleSheet(QString("color:%1; font-size:11px; background:transparent; font-family:%2;")
                                   .arg(colors::TEXT_SECONDARY())
                                   .arg(MONO));
        hl->addWidget(sym_lbl);

        symbol_combo_ = new QComboBox(bar);
        for (int s = 0; s < N_SYMBOLS; s++)
            symbol_combo_->addItem(VOL_SYMBOLS[s]);
        symbol_combo_->setCurrentIndex(selected_symbol_);
        symbol_combo_->setFixedHeight(18);
        symbol_combo_->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                             " padding:0 6px; font-size:11px; font-family:%4; }"
                                             "QComboBox::drop-down { border:none; }"
                                             "QComboBox QAbstractItemView { background:%1; color:%2;"
                                             " border:1px solid %3; selection-background-color:%5; }")
                                         .arg(colors::BG_RAISED())
                                         .arg(colors::TEXT_PRIMARY())
                                         .arg(colors::BORDER_DIM())
                                         .arg(MONO)
                                         .arg(colors::BG_HOVER()));
        connect(symbol_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &SurfaceAnalyticsScreen::on_symbol_changed);
        hl->addWidget(symbol_combo_);
    }

    return bar;
}

void SurfaceAnalyticsScreen::refresh_surface_bar() {
    auto* main_layout = qobject_cast<QVBoxLayout*>(layout());
    if (!main_layout)
        return;
    int idx = main_layout->indexOf(surface_bar_);
    if (idx < 0)
        return;
    main_layout->removeWidget(surface_bar_);
    surface_bar_->deleteLater();
    surface_bar_ = build_surface_bar();
    main_layout->insertWidget(idx, surface_bar_);
}

// ── Data loading ─────────────────────────────────────────────────────────────
void SurfaceAnalyticsScreen::load_demo_data() {
    const char* sym = VOL_SYMBOLS[selected_symbol_];
    float spot = VOL_SPOTS[selected_symbol_];

    vol_data_ = generate_vol_surface(sym, spot);
    delta_data_ = generate_delta_surface(sym, spot);
    gamma_data_ = generate_gamma_surface(sym, spot);
    vega_data_ = generate_vega_surface(sym, spot);
    theta_data_ = generate_theta_surface(sym, spot);
    skew_data_ = generate_skew_surface(sym);
    local_vol_data_ = generate_local_vol(sym, spot);

    yield_data_ = generate_yield_curve();
    swaption_data_ = generate_swaption_vol();
    capfloor_data_ = generate_capfloor_vol();
    bond_spread_data_ = generate_bond_spread();
    ois_data_ = generate_ois_basis();
    real_yield_data_ = generate_real_yield();
    fwd_rate_data_ = generate_forward_rate();

    fx_vol_data_ = generate_fx_vol();
    fx_fwd_data_ = generate_fx_forward_points();
    xccy_data_ = generate_xccy_basis();

    cds_data_ = generate_cds_spread();
    credit_trans_data_ = generate_credit_transition();
    recovery_data_ = generate_recovery_rate();

    cmdty_fwd_data_ = generate_commodity_forward();
    cmdty_vol_data_ = generate_commodity_vol();
    crack_data_ = generate_crack_spread();
    contango_data_ = generate_contango();

    corr_data_ = generate_correlation(corr_assets_);
    pca_data_ = generate_pca(corr_assets_);
    var_data_ = generate_var();
    stress_data_ = generate_stress_test();
    factor_data_ = generate_factor_exposure(corr_assets_);
    liquidity_data_ = generate_liquidity(sym, spot);
    drawdown_data_ = generate_drawdown(corr_assets_);
    beta_data_ = generate_beta(corr_assets_);
    impl_div_data_ = generate_implied_dividend(sym, spot);

    inflation_data_ = generate_inflation_expectations();
    monetary_data_ = generate_monetary_policy();
}

// ── Chart routing ─────────────────────────────────────────────────────────────
void SurfaceAnalyticsScreen::update_chart() {
    auto minmax = [](const std::vector<std::vector<float>>& z, float& mn, float& mx) {
        mn = 9999;
        mx = -9999;
        for (const auto& row : z)
            for (float v : row) {
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
    };

    auto fmt_strikes = [](const std::vector<float>& s) {
        std::vector<std::string> v;
        for (float f : s) {
            char b[16];
            std::snprintf(b, 16, "$%.0f", f);
            v.push_back(b);
        }
        return v;
    };
    auto fmt_dtes = [](const std::vector<int>& s) {
        std::vector<std::string> v;
        for (int i : s) {
            char b[16];
            std::snprintf(b, 16, "%dD", i);
            v.push_back(b);
        }
        return v;
    };
    auto fmt_months = [](const std::vector<int>& s) {
        std::vector<std::string> v;
        for (int i : s) {
            char b[16];
            std::snprintf(b, 16, "%dM", i);
            v.push_back(b);
        }
        return v;
    };

    float mn, mx;

    if (show_table_) {
        view_stack_->setCurrentIndex(1);

        switch (active_chart_) {
            case ChartType::Volatility:
                surface_table_->show_vol(vol_data_);
                return;
            case ChartType::DeltaSurface:
                surface_table_->show_greeks(delta_data_);
                return;
            case ChartType::GammaSurface:
                surface_table_->show_greeks(gamma_data_);
                return;
            case ChartType::VegaSurface:
                surface_table_->show_greeks(vega_data_);
                return;
            case ChartType::ThetaSurface:
                surface_table_->show_greeks(theta_data_);
                return;
            case ChartType::Correlation:
                surface_table_->show_correlation(corr_data_);
                return;
            case ChartType::YieldCurve:
                surface_table_->show_yield(yield_data_);
                return;
            case ChartType::PCA:
                surface_table_->show_pca(pca_data_);
                return;
            default:
                break;
        }

        // Generic table fallback
        std::vector<std::string> r_labels, c_labels;
        const std::vector<std::vector<float>>* zptr = nullptr;
        bool div = false;

        switch (active_chart_) {
            case ChartType::SkewSurface:
                for (int e : skew_data_.expirations)
                    r_labels.push_back(std::to_string(e) + "D");
                for (float d : skew_data_.deltas)
                    c_labels.push_back(std::to_string((int)d) + "D");
                zptr = &skew_data_.z;
                div = true;
                break;
            case ChartType::SwaptionVol:
                for (int e : swaption_data_.option_expiries)
                    r_labels.push_back(std::to_string(e) + "M");
                for (int t : swaption_data_.swap_tenors)
                    c_labels.push_back(std::to_string(t) + "M");
                zptr = &swaption_data_.z;
                break;
            case ChartType::BondSpread:
                r_labels = bond_spread_data_.ratings;
                for (int m : bond_spread_data_.maturities)
                    c_labels.push_back(std::to_string(m) + "M");
                zptr = &bond_spread_data_.z;
                break;
            case ChartType::CDSSpread:
                r_labels = cds_data_.entities;
                for (int t : cds_data_.tenors)
                    c_labels.push_back(std::to_string(t) + "M");
                zptr = &cds_data_.z;
                break;
            case ChartType::CreditTransition:
                r_labels = credit_trans_data_.ratings;
                c_labels = credit_trans_data_.to_ratings;
                zptr = &credit_trans_data_.z;
                div = true;
                break;
            case ChartType::StressTestPnL:
                r_labels = stress_data_.scenarios;
                c_labels = stress_data_.portfolios;
                zptr = &stress_data_.z;
                div = true;
                break;
            case ChartType::FactorExposure:
                r_labels = factor_data_.assets;
                c_labels = factor_data_.factors;
                zptr = &factor_data_.z;
                div = true;
                break;
            case ChartType::Drawdown:
                r_labels = drawdown_data_.assets;
                for (int w : drawdown_data_.windows)
                    c_labels.push_back(std::to_string(w) + "D");
                zptr = &drawdown_data_.z;
                break;
            case ChartType::BetaSurface:
                r_labels = beta_data_.assets;
                for (int h : beta_data_.horizons)
                    c_labels.push_back(std::to_string(h) + "D");
                zptr = &beta_data_.z;
                div = true;
                break;
            case ChartType::CommodityForward:
                r_labels = cmdty_fwd_data_.commodities;
                for (int m : cmdty_fwd_data_.contract_months)
                    c_labels.push_back("M" + std::to_string(m));
                zptr = &cmdty_fwd_data_.z;
                break;
            case ChartType::ContangoBackwardation:
                r_labels = contango_data_.commodities;
                for (int m : contango_data_.contract_months)
                    c_labels.push_back("M" + std::to_string(m));
                zptr = &contango_data_.z;
                div = true;
                break;
            case ChartType::MonetaryPolicyPath:
                r_labels = monetary_data_.central_banks;
                for (int m : monetary_data_.meetings_ahead)
                    c_labels.push_back("Mtg" + std::to_string(m));
                zptr = &monetary_data_.z;
                break;
            default:
                break;
        }

        if (zptr && !zptr->empty()) {
            minmax(*zptr, mn, mx);
            surface_table_->show_generic_matrix(r_labels, c_labels, *zptr, mn, mx, div);
        }
        return;
    }

    // ── 3D mode ───────────────────────────────────────────────────────────────
    view_stack_->setCurrentIndex(0);

    switch (active_chart_) {
        case ChartType::Volatility: {
            minmax(vol_data_.z, mn, mx);
            auto sl = fmt_strikes(vol_data_.strikes), dl = fmt_dtes(vol_data_.expirations);
            surface_3d_->set_surface(vol_data_.z, "STRIKE", "IV %", "DTE", mn, mx, false, &sl, &dl);
            break;
        }
        case ChartType::DeltaSurface: {
            minmax(delta_data_.z, mn, mx);
            auto sl = fmt_strikes(delta_data_.strikes), dl = fmt_dtes(delta_data_.expirations);
            surface_3d_->set_surface(delta_data_.z, "STRIKE", "DELTA", "DTE", mn, mx, false, &sl, &dl);
            break;
        }
        case ChartType::GammaSurface: {
            minmax(gamma_data_.z, mn, mx);
            auto sl = fmt_strikes(gamma_data_.strikes), dl = fmt_dtes(gamma_data_.expirations);
            surface_3d_->set_surface(gamma_data_.z, "STRIKE", "GAMMA", "DTE", mn, mx, false, &sl, &dl);
            break;
        }
        case ChartType::VegaSurface: {
            minmax(vega_data_.z, mn, mx);
            auto sl = fmt_strikes(vega_data_.strikes), dl = fmt_dtes(vega_data_.expirations);
            surface_3d_->set_surface(vega_data_.z, "STRIKE", "VEGA", "DTE", mn, mx, false, &sl, &dl);
            break;
        }
        case ChartType::ThetaSurface: {
            minmax(theta_data_.z, mn, mx);
            auto sl = fmt_strikes(theta_data_.strikes), dl = fmt_dtes(theta_data_.expirations);
            surface_3d_->set_surface(theta_data_.z, "STRIKE", "THETA", "DTE", mn, mx, true, &sl, &dl);
            break;
        }
        case ChartType::SkewSurface: {
            minmax(skew_data_.z, mn, mx);
            std::vector<std::string> dl, sl;
            for (float d : skew_data_.deltas) {
                char b[16];
                std::snprintf(b, 16, "%dD", (int)d);
                dl.push_back(b);
            }
            for (int e : skew_data_.expirations) {
                char b[16];
                std::snprintf(b, 16, "%dD", e);
                sl.push_back(b);
            }
            surface_3d_->set_surface(skew_data_.z, "DELTA", "SKEW %", "DTE", mn, mx, true, &dl, &sl);
            break;
        }
        case ChartType::LocalVolSurface: {
            minmax(local_vol_data_.z, mn, mx);
            auto sl = fmt_strikes(local_vol_data_.strikes), dl = fmt_dtes(local_vol_data_.expirations);
            surface_3d_->set_surface(local_vol_data_.z, "STRIKE", "LV %", "DTE", mn, mx, false, &sl, &dl);
            break;
        }
        case ChartType::YieldCurve: {
            minmax(yield_data_.z, mn, mx);
            std::vector<std::string> ml;
            for (int m : yield_data_.maturities) {
                char b[16];
                std::snprintf(b, 16, "%dM", m);
                ml.push_back(b);
            }
            surface_3d_->set_surface(yield_data_.z, "MATURITY", "YIELD %", "TIME", mn, mx, false, &ml, nullptr);
            break;
        }
        case ChartType::SwaptionVol: {
            minmax(swaption_data_.z, mn, mx);
            auto tl = fmt_months(swaption_data_.swap_tenors), el = fmt_months(swaption_data_.option_expiries);
            surface_3d_->set_surface(swaption_data_.z, "TENOR", "VOL bp", "EXPIRY", mn, mx, false, &tl, &el);
            break;
        }
        case ChartType::CapFloorVol: {
            minmax(capfloor_data_.z, mn, mx);
            auto tl = fmt_months(capfloor_data_.maturities);
            auto sl = fmt_strikes(capfloor_data_.strikes);
            surface_3d_->set_surface(capfloor_data_.z, "MATURITY", "VOL bp", "STRIKE", mn, mx, false, &tl, &sl);
            break;
        }
        case ChartType::BondSpread: {
            minmax(bond_spread_data_.z, mn, mx);
            auto ml = fmt_months(bond_spread_data_.maturities);
            surface_3d_->set_surface(bond_spread_data_.z, "MATURITY", "SPREAD bp", "RATING", mn, mx, false, &ml,
                                     nullptr);
            break;
        }
        case ChartType::OISBasis: {
            minmax(ois_data_.z, mn, mx);
            auto tl = fmt_months(ois_data_.tenors);
            surface_3d_->set_surface(ois_data_.z, "TENOR", "BASIS bp", "TIME", mn, mx, true, &tl, nullptr);
            break;
        }
        case ChartType::RealYield: {
            minmax(real_yield_data_.z, mn, mx);
            auto ml = fmt_months(real_yield_data_.maturities);
            surface_3d_->set_surface(real_yield_data_.z, "MATURITY", "REAL YLD %", "TIME", mn, mx, true, &ml, nullptr);
            break;
        }
        case ChartType::ForwardRate: {
            minmax(fwd_rate_data_.z, mn, mx);
            auto tl = fmt_months(fwd_rate_data_.start_tenors);
            auto fl = fmt_months(fwd_rate_data_.forward_periods);
            surface_3d_->set_surface(fwd_rate_data_.z, "START TENOR", "FWD RATE %", "FWD PERIOD", mn, mx, false, &tl,
                                     &fl);
            break;
        }
        case ChartType::FXVol: {
            minmax(fx_vol_data_.z, mn, mx);
            std::vector<std::string> dl, tl;
            for (float d : fx_vol_data_.deltas) {
                char b[16];
                std::snprintf(b, 16, "%dD", (int)d);
                dl.push_back(b);
            }
            for (int t : fx_vol_data_.tenors) {
                char b[16];
                std::snprintf(b, 16, "%dD", t);
                tl.push_back(b);
            }
            surface_3d_->set_surface(fx_vol_data_.z, "TENOR", "VOL %", "DELTA", mn, mx, false, &dl, &tl);
            break;
        }
        case ChartType::FXForwardPoints: {
            minmax(fx_fwd_data_.z, mn, mx);
            auto tl = fmt_months(fx_fwd_data_.tenors);
            surface_3d_->set_surface(fx_fwd_data_.z, "TENOR", "FWD PTS", "PAIR", mn, mx, true, &tl, nullptr);
            break;
        }
        case ChartType::CrossCurrencyBasis: {
            minmax(xccy_data_.z, mn, mx);
            auto tl = fmt_months(xccy_data_.tenors);
            surface_3d_->set_surface(xccy_data_.z, "TENOR", "BASIS bp", "PAIR", mn, mx, true, &tl, nullptr);
            break;
        }
        case ChartType::CDSSpread: {
            minmax(cds_data_.z, mn, mx);
            auto tl = fmt_months(cds_data_.tenors);
            surface_3d_->set_surface(cds_data_.z, "TENOR", "SPREAD bp", "ENTITY", mn, mx, false, &tl, nullptr);
            break;
        }
        case ChartType::CreditTransition: {
            minmax(credit_trans_data_.z, mn, mx);
            surface_3d_->set_surface(credit_trans_data_.z, "TO RATING", "PROB %", "FROM RATING", mn, mx, false);
            break;
        }
        case ChartType::RecoveryRate: {
            minmax(recovery_data_.z, mn, mx);
            surface_3d_->set_surface(recovery_data_.z, "SECTOR", "RECOVERY %", "SENIORITY", mn, mx, false);
            break;
        }
        case ChartType::CommodityForward: {
            minmax(cmdty_fwd_data_.z, mn, mx);
            std::vector<std::string> ml;
            for (int m : cmdty_fwd_data_.contract_months) {
                char b[8];
                std::snprintf(b, 8, "M%d", m);
                ml.push_back(b);
            }
            surface_3d_->set_surface(cmdty_fwd_data_.z, "CONTRACT", "PRICE", "COMMODITY", mn, mx, false, &ml, nullptr);
            break;
        }
        case ChartType::CommodityVol: {
            minmax(cmdty_vol_data_.z, mn, mx);
            auto sl = fmt_strikes(cmdty_vol_data_.strikes);
            auto el = fmt_dtes(cmdty_vol_data_.expirations);
            surface_3d_->set_surface(cmdty_vol_data_.z, "STRIKE", "VOL %", "EXPIRY", mn, mx, false, &sl, &el);
            break;
        }
        case ChartType::CrackSpread: {
            minmax(crack_data_.z, mn, mx);
            std::vector<std::string> ml;
            for (int m : crack_data_.contract_months) {
                char b[8];
                std::snprintf(b, 8, "M%d", m);
                ml.push_back(b);
            }
            surface_3d_->set_surface(crack_data_.z, "CONTRACT", "SPREAD $/bbl", "PRODUCT", mn, mx, true, &ml, nullptr);
            break;
        }
        case ChartType::ContangoBackwardation: {
            minmax(contango_data_.z, mn, mx);
            std::vector<std::string> ml;
            for (int m : contango_data_.contract_months) {
                char b[8];
                std::snprintf(b, 8, "M%d", m);
                ml.push_back(b);
            }
            surface_3d_->set_surface(contango_data_.z, "CONTRACT", "ROLL %", "COMMODITY", mn, mx, true, &ml, nullptr);
            break;
        }
        case ChartType::Correlation: {
            int n = (int)corr_data_.assets.size();
            if (corr_data_.z.empty() || (int)corr_data_.z.size() < n)
                break;
            std::vector<std::vector<float>> slice(n, std::vector<float>(n));
            for (int r = 0; r < n; r++)
                for (int c = 0; c < n; c++)
                    slice[r][c] = corr_data_.z[r][c];
            surface_3d_->set_surface(slice, "ASSET", "CORR", "ASSET", -1.f, 1.f, true);
            break;
        }
        case ChartType::PCA: {
            minmax(pca_data_.z, mn, mx);
            surface_3d_->set_surface(pca_data_.z, "ASSET", "LOADING", "PC", mn, mx, true);
            break;
        }
        case ChartType::VaR: {
            minmax(var_data_.z, mn, mx);
            surface_3d_->set_surface(var_data_.z, "HORIZON", "VaR %", "CONFIDENCE", mn, mx, false);
            break;
        }
        case ChartType::StressTestPnL: {
            minmax(stress_data_.z, mn, mx);
            surface_3d_->set_surface(stress_data_.z, "PORTFOLIO", "P&L %", "SCENARIO", mn, mx, true);
            break;
        }
        case ChartType::FactorExposure: {
            minmax(factor_data_.z, mn, mx);
            surface_3d_->set_surface(factor_data_.z, "FACTOR", "EXPOSURE", "ASSET", mn, mx, true);
            break;
        }
        case ChartType::LiquidityHeatmap: {
            minmax(liquidity_data_.z, mn, mx);
            surface_3d_->set_surface(liquidity_data_.z, "TIME", "BID-ASK bp", "STRIKE", mn, mx, false);
            break;
        }
        case ChartType::Drawdown: {
            minmax(drawdown_data_.z, mn, mx);
            std::vector<std::string> wl;
            for (int w : drawdown_data_.windows) {
                char b[8];
                std::snprintf(b, 8, "%dD", w);
                wl.push_back(b);
            }
            surface_3d_->set_surface(drawdown_data_.z, "WINDOW", "DRAWDOWN %", "ASSET", mn, mx, false, &wl, nullptr);
            break;
        }
        case ChartType::BetaSurface: {
            minmax(beta_data_.z, mn, mx);
            std::vector<std::string> hl2;
            for (int h : beta_data_.horizons) {
                char b[8];
                std::snprintf(b, 8, "%dD", h);
                hl2.push_back(b);
            }
            surface_3d_->set_surface(beta_data_.z, "HORIZON", "BETA", "ASSET", mn, mx, true, &hl2, nullptr);
            break;
        }
        case ChartType::ImpliedDividend: {
            minmax(impl_div_data_.z, mn, mx);
            auto el = fmt_dtes(impl_div_data_.expirations);
            surface_3d_->set_surface(impl_div_data_.z, "EXPIRY", "DIV $", "SERIES", mn, mx, false, &el, nullptr);
            break;
        }
        case ChartType::InflationExpectations: {
            minmax(inflation_data_.z, mn, mx);
            std::vector<std::string> hl;
            for (int h : inflation_data_.horizons) {
                char b[8];
                std::snprintf(b, 8, "%dY", h);
                hl.push_back(b);
            }
            surface_3d_->set_surface(inflation_data_.z, "HORIZON", "BREAKEVEN %", "TIME", mn, mx, false, &hl, nullptr);
            break;
        }
        case ChartType::MonetaryPolicyPath: {
            minmax(monetary_data_.z, mn, mx);
            std::vector<std::string> ml;
            for (int m : monetary_data_.meetings_ahead) {
                char b[8];
                std::snprintf(b, 8, "Mtg%d", m);
                ml.push_back(b);
            }
            surface_3d_->set_surface(monetary_data_.z, "MEETING", "RATE %", "CENTRAL BANK", mn, mx, false, &ml,
                                     nullptr);
            break;
        }
        default:
            surface_3d_->clear();
            break;
    }
}

// ── Metrics routing ───────────────────────────────────────────────────────────
void SurfaceAnalyticsScreen::update_metrics() {
    metrics_panel_->update_metrics(
        active_chart_, vol_data_, delta_data_, gamma_data_, vega_data_, theta_data_, skew_data_, local_vol_data_,
        yield_data_, swaption_data_, capfloor_data_, bond_spread_data_, ois_data_, real_yield_data_, fwd_rate_data_,
        fx_vol_data_, fx_fwd_data_, xccy_data_, cds_data_, credit_trans_data_, recovery_data_, cmdty_fwd_data_,
        cmdty_vol_data_, crack_data_, contango_data_, corr_data_, pca_data_, var_data_, stress_data_, factor_data_,
        liquidity_data_, drawdown_data_, beta_data_, impl_div_data_, inflation_data_, monetary_data_);
}

// ── Slots ─────────────────────────────────────────────────────────────────────
void SurfaceAnalyticsScreen::on_category_clicked(int index) {
    active_category_ = index;
    const auto cats = get_surface_categories();
    if (!cats.empty() && index < (int)cats.size())
        active_chart_ = cats[index].types[0];

    // Rebuild both bars
    auto* main_layout = qobject_cast<QVBoxLayout*>(layout());
    if (main_layout) {
        int ci = main_layout->indexOf(category_bar_);
        int si = main_layout->indexOf(surface_bar_);
        if (ci >= 0) {
            main_layout->removeWidget(category_bar_);
            category_bar_->deleteLater();
            category_bar_ = build_category_bar();
            main_layout->insertWidget(ci, category_bar_);
        }
        if (si >= 0) {
            main_layout->removeWidget(surface_bar_);
            surface_bar_->deleteLater();
            surface_bar_ = build_surface_bar();
            main_layout->insertWidget(si, surface_bar_);
        }
    }

    databento_panel_->set_active_chart(active_chart_, VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    update_chart();
    update_metrics();
    fincept::ScreenStateManager::instance().notify_changed(this);
}

void SurfaceAnalyticsScreen::on_surface_clicked(int cat, int surf_index) {
    const auto cats = get_surface_categories();
    if (cat < (int)cats.size() && surf_index < (int)cats[cat].types.size())
        active_chart_ = cats[cat].types[surf_index];
    refresh_surface_bar();
    databento_panel_->set_active_chart(active_chart_, VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_view_3d() {
    show_table_ = false;
    btn_3d_->setStyleSheet(btn_active_amber());
    btn_table_->setStyleSheet(btn_inactive());
    update_chart();
}

void SurfaceAnalyticsScreen::on_view_table() {
    show_table_ = true;
    btn_3d_->setStyleSheet(btn_inactive());
    btn_table_->setStyleSheet(btn_active_amber());
    update_chart();
}

void SurfaceAnalyticsScreen::on_import_csv() {
    QString path = QFileDialog::getOpenFileName(this, "Import Surface CSV", {}, "CSV Files (*.csv)");
    if (!path.isEmpty())
        dispatch_csv(path);
}

void SurfaceAnalyticsScreen::on_refresh() {
    load_demo_data();
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_symbol_changed(int index) {
    selected_symbol_ = index;
    load_demo_data();
    update_chart();
    update_metrics();
    databento_panel_->set_active_chart(active_chart_, VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
}

void SurfaceAnalyticsScreen::dispatch_csv(const QString& path) {
    std::string err;
    auto rows = parse_csv_file(path, err);
    if (rows.empty())
        return;
    switch (active_chart_) {
        case ChartType::Volatility:
            if (load_vol_surface(rows, vol_data_, err)) {
                update_chart();
                update_metrics();
            }
            break;
        case ChartType::DeltaSurface:
            if (load_greeks_surface(rows, delta_data_, err, "Delta")) {
                update_chart();
                update_metrics();
            }
            break;
        case ChartType::GammaSurface:
            if (load_greeks_surface(rows, gamma_data_, err, "Gamma")) {
                update_chart();
                update_metrics();
            }
            break;
        case ChartType::VegaSurface:
            if (load_greeks_surface(rows, vega_data_, err, "Vega")) {
                update_chart();
                update_metrics();
            }
            break;
        case ChartType::ThetaSurface:
            if (load_greeks_surface(rows, theta_data_, err, "Theta")) {
                update_chart();
                update_metrics();
            }
            break;
        default:
            break;
    }
}

// ── Databento slots ───────────────────────────────────────────────────────────
void SurfaceAnalyticsScreen::on_vol_surface_received(const fincept::DatabentoVolSurfaceResult& r) {
    if (r.success) {
        const char* sym = VOL_SYMBOLS[selected_symbol_];
        float spot = VOL_SPOTS[selected_symbol_];
        if (!r.vol.z.empty()) {
            vol_data_ = r.vol;
            vol_data_.underlying = sym;
            vol_data_.spot_price = spot;
        }
        if (!r.delta.z.empty()) {
            delta_data_ = r.delta;
            delta_data_.underlying = sym;
            delta_data_.spot_price = spot;
        }
        if (!r.gamma.z.empty()) {
            gamma_data_ = r.gamma;
            gamma_data_.underlying = sym;
            gamma_data_.spot_price = spot;
        }
        if (!r.vega.z.empty()) {
            vega_data_ = r.vega;
            vega_data_.underlying = sym;
            vega_data_.spot_price = spot;
        }
        if (!r.theta.z.empty()) {
            theta_data_ = r.theta;
            theta_data_.underlying = sym;
            theta_data_.spot_price = spot;
        }
        if (!r.skew.z.empty()) {
            skew_data_ = r.skew;
            skew_data_.underlying = sym;
        }
    }
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_ohlcv_received(const fincept::DatabentoOhlcvResult&) {
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_futures_received(const fincept::DatabentoFuturesResult& r) {
    if (r.success) {
        if (!r.forward.z.empty())
            cmdty_fwd_data_ = r.forward;
        if (!r.contango.z.empty())
            contango_data_ = r.contango;
    }
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_surface_received(const fincept::DatabentoSurfaceResult& r) {
    if (!r.success) {
        update_chart();
        return;
    }
    const auto& type = r.type;

    if (type == "local_vol" && !r.z.empty()) {
        local_vol_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        local_vol_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        local_vol_data_.z = r.z;
        local_vol_data_.underlying = VOL_SYMBOLS[selected_symbol_];
        local_vol_data_.spot_price = VOL_SPOTS[selected_symbol_];
    } else if (type == "implied_dividend" && !r.z.empty()) {
        impl_div_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        impl_div_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        impl_div_data_.z = r.z;
        impl_div_data_.underlying = VOL_SYMBOLS[selected_symbol_];
    } else if (type == "liquidity" && !r.z.empty()) {
        liquidity_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        liquidity_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        liquidity_data_.z = r.z;
        liquidity_data_.underlying = VOL_SYMBOLS[selected_symbol_];
    } else if (type == "commodity_vol" && !r.z.empty()) {
        cmdty_vol_data_.strikes.assign(r.x_axis.begin(), r.x_axis.end());
        cmdty_vol_data_.expirations.assign(r.y_axis.begin(), r.y_axis.end());
        cmdty_vol_data_.z = r.z;
        cmdty_vol_data_.commodity = "CL";
    } else if (type == "crack_spread" && !r.z.empty()) {
        crack_data_.spread_types = r.x_labels;
        crack_data_.contract_months.assign(r.y_axis.begin(), r.y_axis.end());
        crack_data_.z = r.z;
    } else if (type == "stress_test" && !r.z.empty()) {
        stress_data_.scenarios = r.x_labels;
        stress_data_.portfolios = r.y_labels;
        stress_data_.z = r.z;
    } else if (type == "yield_curve" && !r.z.empty()) {
        yield_data_.maturities.clear();
        for (float f : r.x_axis)
            yield_data_.maturities.push_back((int)f);
        yield_data_.time_points.assign(r.y_axis.begin(), r.y_axis.end());
        yield_data_.z = r.z;
    } else if (type == "forward_rate" && !r.z.empty()) {
        fwd_rate_data_.start_tenors.clear();
        for (float f : r.x_axis)
            fwd_rate_data_.start_tenors.push_back((int)f);
        fwd_rate_data_.forward_periods.assign(r.y_axis.begin(), r.y_axis.end());
        fwd_rate_data_.z = r.z;
    } else if (type == "rate_path" && !r.z.empty()) {
        monetary_data_.central_banks = r.x_labels;
        monetary_data_.meetings_ahead.assign(r.y_axis.begin(), r.y_axis.end());
        monetary_data_.z = r.z;
    } else if (type == "fx_forward_points" && !r.z.empty()) {
        fx_fwd_data_.pairs = r.x_labels;
        fx_fwd_data_.tenors.assign(r.y_axis.begin(), r.y_axis.end());
        fx_fwd_data_.z = r.z;
    }

    update_chart();
    update_metrics();
}

// ── Show/hide event — P3 compliance ──────────────────────────────────────────
void SurfaceAnalyticsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    connect(databento_panel_, &SurfaceDatabentoPanel::vol_surface_received, this,
            &SurfaceAnalyticsScreen::on_vol_surface_received, Qt::UniqueConnection);
    connect(databento_panel_, &SurfaceDatabentoPanel::ohlcv_received, this, &SurfaceAnalyticsScreen::on_ohlcv_received,
            Qt::UniqueConnection);
    connect(databento_panel_, &SurfaceDatabentoPanel::futures_received, this,
            &SurfaceAnalyticsScreen::on_futures_received, Qt::UniqueConnection);
    connect(databento_panel_, &SurfaceDatabentoPanel::surface_received, this,
            &SurfaceAnalyticsScreen::on_surface_received, Qt::UniqueConnection);
}

void SurfaceAnalyticsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    disconnect(databento_panel_, nullptr, this, nullptr);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap SurfaceAnalyticsScreen::save_state() const {
    return {
        {"category", active_category_},
        {"chart", static_cast<int>(active_chart_)},
    };
}

void SurfaceAnalyticsScreen::restore_state(const QVariantMap& state) {
    const int cat = state.value("category", 0).toInt();
    if (cat != active_category_)
        on_category_clicked(cat);
}

// ── IGroupLinked ─────────────────────────────────────────────────────────────

void SurfaceAnalyticsScreen::on_group_symbol_changed(const fincept::SymbolRef& ref) {
    if (!ref.is_valid() || !symbol_combo_)
        return;
    // Only act when the incoming symbol matches one of our supported tickers
    // (VOL_SYMBOLS). For anything else the screen has no data surface, so
    // silently ignore rather than clearing the current view.
    for (int i = 0; i < N_SYMBOLS; ++i) {
        if (ref.symbol.compare(QString::fromLatin1(VOL_SYMBOLS[i]), Qt::CaseInsensitive) == 0) {
            if (symbol_combo_->currentIndex() != i)
                symbol_combo_->setCurrentIndex(i); // triggers on_symbol_changed
            return;
        }
    }
}

fincept::SymbolRef SurfaceAnalyticsScreen::current_symbol() const {
    if (selected_symbol_ < 0 || selected_symbol_ >= N_SYMBOLS)
        return {};
    return fincept::SymbolRef::equity(QString::fromLatin1(VOL_SYMBOLS[selected_symbol_]));
}

} // namespace fincept::surface
