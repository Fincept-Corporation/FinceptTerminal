#include "SurfaceAnalyticsScreen.h"
#include "Surface3DWidget.h"
#include "SurfaceTableWidget.h"
#include "SurfaceMetricsPanel.h"
#include "SurfaceDatabentoPanel.h"
#include "SurfaceCsvImporter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QStackedWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFrame>
#include <QScrollArea>
#include <cstdlib>
#include <ctime>

namespace fincept::surface {

// ============================================================================
SurfaceAnalyticsScreen::SurfaceAnalyticsScreen(QWidget* parent) : QWidget(parent) {
    srand((unsigned)time(nullptr));
    setup_ui();
    load_demo_data();
    update_chart();
    update_metrics();
}

// ============================================================================
void SurfaceAnalyticsScreen::setup_ui() {
    setStyleSheet("background:#0d1117; color:#c9d1d9;");
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0,0,0,0);
    main_layout->setSpacing(0);

    // ── Row 1: category tabs + controls ──────────────────────────────────────
    category_bar_ = build_category_bar();
    main_layout->addWidget(category_bar_);

    // ── Row 2: surface chips ──────────────────────────────────────────────────
    surface_bar_ = build_surface_bar();
    main_layout->addWidget(surface_bar_);

    // ── Content: metrics + chart ──────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet("QSplitter::handle { background:#21262d; }");

    metrics_panel_ = new SurfaceMetricsPanel(splitter);
    splitter->addWidget(metrics_panel_);

    // Right side: chart on top, Databento panel below
    auto* right_widget = new QWidget(splitter);
    auto* right_vl = new QVBoxLayout(right_widget);
    right_vl->setContentsMargins(0,0,0,0);
    right_vl->setSpacing(0);

    // View stack: 3D widget or table widget
    view_stack_ = new QStackedWidget(right_widget);

    surface_3d_ = new Surface3DWidget(view_stack_);
    view_stack_->addWidget(surface_3d_);   // index 0

    surface_table_ = new SurfaceTableWidget(view_stack_);
    view_stack_->addWidget(surface_table_); // index 1

    right_vl->addWidget(view_stack_, 1);

    // Databento panel — collapsible strip at bottom of chart area
    databento_panel_ = new SurfaceDatabentoPanel(right_widget);
    databento_panel_->setFixedHeight(220);
    right_vl->addWidget(databento_panel_);

    splitter->addWidget(right_widget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    main_layout->addWidget(splitter, 1);
}

// ── Category bar ─────────────────────────────────────────────────────────────
QWidget* SurfaceAnalyticsScreen::build_category_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(36);
    bar->setStyleSheet("background:#010409; border-bottom:1px solid #21262d;");
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8,0,8,0);
    hl->setSpacing(2);

    const auto categories = get_surface_categories();
    for (int i = 0; i < (int)categories.size(); i++) {
        auto* btn = new QPushButton(categories[i].name, bar);
        int r = CAT_COLORS[i][0], g = CAT_COLORS[i][1], b_c = CAT_COLORS[i][2];
        bool active = (i == active_category_);
        btn->setStyleSheet(QString(
            "QPushButton { background:%1; color:%2; border:none; padding:4px 12px; "
            "font-size:10px; font-weight:bold; border-radius:2px; }"
            "QPushButton:hover { background:%3; }")
            .arg(active ? QString("rgba(%1,%2,%3,40)").arg(r).arg(g).arg(b_c) : "transparent")
            .arg(active ? QString("rgb(%1,%2,%3)").arg(r).arg(g).arg(b_c) : "#8b949e")
            .arg(QString("rgba(%1,%2,%3,20)").arg(r).arg(g).arg(b_c)));
        btn->setProperty("cat_index", i);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_category_clicked(i); });
        hl->addWidget(btn);
    }

    hl->addStretch();

    // IMPORT button
    auto* import_btn = new QPushButton("IMPORT CSV", bar);
    import_btn->setStyleSheet(
        "QPushButton { background:rgba(31,77,153,255); color:#88bbff; border:1px solid rgba(64,140,255,100);"
        " padding:4px 12px; font-size:10px; border-radius:2px; }"
        "QPushButton:hover { background:rgba(46,102,191,255); }");
    connect(import_btn, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_import_csv);
    hl->addWidget(import_btn);

    // 3D/TABLE toggle
    btn_3d_ = new QPushButton("3D", bar);
    btn_table_ = new QPushButton("TABLE", bar);
    for (auto* b : {btn_3d_, btn_table_}) {
        b->setCheckable(true);
        b->setStyleSheet(
            "QPushButton { background:#161b22; color:#8b949e; border:1px solid #30363d;"
            " padding:4px 10px; font-size:10px; border-radius:2px; }"
            "QPushButton:checked { background:rgba(51,153,255,30); color:#3399ff; border-color:rgba(51,153,255,100); }"
            "QPushButton:hover { background:#21262d; }");
    }
    btn_3d_->setChecked(true);
    connect(btn_3d_,    &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_view_3d);
    connect(btn_table_, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_view_table);
    hl->addWidget(btn_3d_);
    hl->addWidget(btn_table_);

    // REFRESH
    auto* ref_btn = new QPushButton("REFRESH", bar);
    ref_btn->setStyleSheet(
        "QPushButton { background:transparent; color:#3399ff; border:1px solid #30363d;"
        " padding:4px 10px; font-size:10px; border-radius:2px; }"
        "QPushButton:hover { background:#21262d; }");
    connect(ref_btn, &QPushButton::clicked, this, &SurfaceAnalyticsScreen::on_refresh);
    hl->addWidget(ref_btn);

    return bar;
}

// ── Surface chip bar ──────────────────────────────────────────────────────────
QWidget* SurfaceAnalyticsScreen::build_surface_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(32);
    bar->setStyleSheet("background:#0d1117; border-bottom:1px solid #21262d;");
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8,0,8,0);
    hl->setSpacing(3);

    const auto categories = get_surface_categories();
    if (active_category_ >= (int)categories.size()) return bar;

    const auto& cat = categories[active_category_];
    int r = CAT_COLORS[active_category_][0];
    int g = CAT_COLORS[active_category_][1];
    int b_c = CAT_COLORS[active_category_][2];

    for (int i = 0; i < (int)cat.types.size(); i++) {
        bool act = (cat.types[i] == active_chart_);
        const char* name = chart_type_name(cat.types[i]);
        auto* btn = new QPushButton(name, bar);
        btn->setStyleSheet(QString(
            "QPushButton { background:%1; color:%2; border:%3; padding:3px 10px;"
            " font-size:9px; border-radius:3px; }"
            "QPushButton:hover { background:rgba(%4,%5,%6,20); }")
            .arg(act ? QString("rgba(%1,%2,%3,40)").arg(r).arg(g).arg(b_c) : "#161b22")
            .arg(act ? QString("rgb(%1,%2,%3)").arg(r).arg(g).arg(b_c) : "#8b949e")
            .arg(act ? QString("1px solid rgba(%1,%2,%3,120)").arg(r).arg(g).arg(b_c) : "1px solid #21262d")
            .arg(r).arg(g).arg(b_c));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_surface_clicked(active_category_, i); });
        hl->addWidget(btn);
    }

    hl->addStretch();

    // Symbol selector (only for equity derivatives)
    if (active_category_ == 0) {
        symbol_combo_ = new QComboBox(bar);
        for (int s = 0; s < N_SYMBOLS; s++)
            symbol_combo_->addItem(VOL_SYMBOLS[s]);
        symbol_combo_->setCurrentIndex(selected_symbol_);
        symbol_combo_->setStyleSheet(
            "QComboBox { background:#161b22; color:#c9d1d9; border:1px solid #30363d;"
            " padding:2px 6px; font-size:10px; border-radius:2px; }"
            "QComboBox::drop-down { border:none; }"
            "QComboBox QAbstractItemView { background:#161b22; color:#c9d1d9; selection-background-color:#21262d; }");
        connect(symbol_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SurfaceAnalyticsScreen::on_symbol_changed);
        hl->addWidget(symbol_combo_);
    }

    return bar;
}

void SurfaceAnalyticsScreen::refresh_surface_bar() {
    // Replace old surface bar with new one
    auto* main_layout = qobject_cast<QVBoxLayout*>(layout());
    if (!main_layout) return;
    int idx = main_layout->indexOf(surface_bar_);
    if (idx < 0) return;
    main_layout->removeWidget(surface_bar_);
    surface_bar_->deleteLater();
    surface_bar_ = build_surface_bar();
    main_layout->insertWidget(idx, surface_bar_);
}

// ============================================================================
void SurfaceAnalyticsScreen::load_demo_data() {
    const char* sym = VOL_SYMBOLS[selected_symbol_];
    float spot = VOL_SPOTS[selected_symbol_];

    vol_data_       = generate_vol_surface(sym, spot);
    delta_data_     = generate_delta_surface(sym, spot);
    gamma_data_     = generate_gamma_surface(sym, spot);
    vega_data_      = generate_vega_surface(sym, spot);
    theta_data_     = generate_theta_surface(sym, spot);
    skew_data_      = generate_skew_surface(sym);
    local_vol_data_ = generate_local_vol(sym, spot);

    yield_data_       = generate_yield_curve();
    swaption_data_    = generate_swaption_vol();
    capfloor_data_    = generate_capfloor_vol();
    bond_spread_data_ = generate_bond_spread();
    ois_data_         = generate_ois_basis();
    real_yield_data_  = generate_real_yield();
    fwd_rate_data_    = generate_forward_rate();

    fx_vol_data_  = generate_fx_vol();
    fx_fwd_data_  = generate_fx_forward_points();
    xccy_data_    = generate_xccy_basis();

    cds_data_          = generate_cds_spread();
    credit_trans_data_ = generate_credit_transition();
    recovery_data_     = generate_recovery_rate();

    cmdty_fwd_data_ = generate_commodity_forward();
    cmdty_vol_data_ = generate_commodity_vol();
    crack_data_     = generate_crack_spread();
    contango_data_  = generate_contango();

    corr_data_     = generate_correlation(corr_assets_);
    pca_data_      = generate_pca(corr_assets_);
    var_data_      = generate_var();
    stress_data_   = generate_stress_test();
    factor_data_   = generate_factor_exposure(corr_assets_);
    liquidity_data_= generate_liquidity(sym, spot);
    drawdown_data_ = generate_drawdown(corr_assets_);
    beta_data_     = generate_beta(corr_assets_);
    impl_div_data_ = generate_implied_dividend(sym, spot);

    inflation_data_ = generate_inflation_expectations();
    monetary_data_  = generate_monetary_policy();
}

// ============================================================================
// Dispatch active surface to 3D widget
void SurfaceAnalyticsScreen::update_chart() {
    if (show_table_) {
        view_stack_->setCurrentIndex(1);
        // Route to table
        float mn, mx;
        auto minmax = [&](const std::vector<std::vector<float>>& z) {
            mn=9999; mx=-9999;
            for (const auto& r:z) for (float v:r){mn=std::min(mn,v);mx=std::max(mx,v);}
        };
        switch (active_chart_) {
        case ChartType::Volatility:
            surface_table_->show_vol(vol_data_); break;
        case ChartType::DeltaSurface: surface_table_->show_greeks(delta_data_); break;
        case ChartType::GammaSurface: surface_table_->show_greeks(gamma_data_); break;
        case ChartType::VegaSurface:  surface_table_->show_greeks(vega_data_);  break;
        case ChartType::ThetaSurface: surface_table_->show_greeks(theta_data_); break;
        case ChartType::Correlation:  surface_table_->show_correlation(corr_data_); break;
        case ChartType::YieldCurve:   surface_table_->show_yield(yield_data_); break;
        case ChartType::PCA:          surface_table_->show_pca(pca_data_); break;
        default: {
            // Generic — build row/col label vectors
            std::vector<std::string> r_labels, c_labels;
            const std::vector<std::vector<float>>* zptr = nullptr;
            bool div = false;
            switch (active_chart_) {
            case ChartType::SkewSurface:
                for (int e:skew_data_.expirations) r_labels.push_back(std::to_string(e)+"D");
                for (float d:skew_data_.deltas) c_labels.push_back(std::to_string((int)d)+"D");
                zptr=&skew_data_.z; div=true; break;
            case ChartType::SwaptionVol:
                for (int e:swaption_data_.option_expiries) r_labels.push_back(std::to_string(e)+"M");
                for (int t:swaption_data_.swap_tenors) c_labels.push_back(std::to_string(t)+"M");
                zptr=&swaption_data_.z; break;
            case ChartType::BondSpread:
                r_labels=bond_spread_data_.ratings;
                for (int m:bond_spread_data_.maturities) c_labels.push_back(std::to_string(m)+"M");
                zptr=&bond_spread_data_.z; break;
            case ChartType::CDSSpread:
                r_labels=cds_data_.entities;
                for (int t:cds_data_.tenors) c_labels.push_back(std::to_string(t)+"M");
                zptr=&cds_data_.z; break;
            case ChartType::CreditTransition:
                r_labels=credit_trans_data_.ratings;
                c_labels=credit_trans_data_.to_ratings;
                zptr=&credit_trans_data_.z; div=true; break;
            case ChartType::StressTestPnL:
                r_labels=stress_data_.scenarios;
                c_labels=stress_data_.portfolios;
                zptr=&stress_data_.z; div=true; break;
            case ChartType::FactorExposure:
                r_labels=factor_data_.assets;
                c_labels=factor_data_.factors;
                zptr=&factor_data_.z; div=true; break;
            case ChartType::Drawdown:
                r_labels=drawdown_data_.assets;
                for (int w:drawdown_data_.windows) c_labels.push_back(std::to_string(w)+"D");
                zptr=&drawdown_data_.z; break;
            case ChartType::BetaSurface:
                r_labels=beta_data_.assets;
                for (int h:beta_data_.horizons) c_labels.push_back(std::to_string(h)+"D");
                zptr=&beta_data_.z; div=true; break;
            case ChartType::CommodityForward:
                r_labels=cmdty_fwd_data_.commodities;
                for (int m:cmdty_fwd_data_.contract_months) c_labels.push_back("M"+std::to_string(m));
                zptr=&cmdty_fwd_data_.z; break;
            case ChartType::ContangoBackwardation:
                r_labels=contango_data_.commodities;
                for (int m:contango_data_.contract_months) c_labels.push_back("M"+std::to_string(m));
                zptr=&contango_data_.z; div=true; break;
            case ChartType::MonetaryPolicyPath:
                r_labels=monetary_data_.central_banks;
                for (int m:monetary_data_.meetings_ahead) c_labels.push_back("Mtg"+std::to_string(m));
                zptr=&monetary_data_.z; break;
            default: break;
            }
            if (zptr && !zptr->empty()) {
                minmax(*zptr);
                surface_table_->show_generic_matrix(r_labels, c_labels, *zptr, mn, mx, div);
            }
            break;
        }
        }
        return;
    }

    // ---- 3D mode ----
    view_stack_->setCurrentIndex(0);

    char lb[32];
    auto fmt_strikes = [](const std::vector<float>& s) {
        std::vector<std::string> v;
        for (float f : s) { char b[16]; std::snprintf(b,16,"$%.0f",f); v.push_back(b); }
        return v;
    };
    auto fmt_dtes = [](const std::vector<int>& s) {
        std::vector<std::string> v;
        for (int i : s) { char b[16]; std::snprintf(b,16,"%dD",i); v.push_back(b); }
        return v;
    };
    auto fmt_months = [](const std::vector<int>& s) {
        std::vector<std::string> v;
        for (int i : s) { char b[16]; std::snprintf(b,16,"%dM",i); v.push_back(b); }
        return v;
    };
    auto minmax = [](const std::vector<std::vector<float>>& z, float& mn, float& mx) {
        mn=9999; mx=-9999;
        for (const auto& r:z) for (float v:r){mn=std::min(mn,v);mx=std::max(mx,v);}
    };
    float mn,mx;

    switch (active_chart_) {
    case ChartType::Volatility: {
        minmax(vol_data_.z,mn,mx);
        auto sl=fmt_strikes(vol_data_.strikes), dl=fmt_dtes(vol_data_.expirations);
        surface_3d_->set_surface(vol_data_.z,"STRIKE","IV %","DTE",mn,mx,false,&sl,&dl); break;
    }
    case ChartType::DeltaSurface: {
        minmax(delta_data_.z,mn,mx);
        auto sl=fmt_strikes(delta_data_.strikes), dl=fmt_dtes(delta_data_.expirations);
        surface_3d_->set_surface(delta_data_.z,"STRIKE","DELTA","DTE",mn,mx,false,&sl,&dl); break;
    }
    case ChartType::GammaSurface: {
        minmax(gamma_data_.z,mn,mx);
        auto sl=fmt_strikes(gamma_data_.strikes), dl=fmt_dtes(gamma_data_.expirations);
        surface_3d_->set_surface(gamma_data_.z,"STRIKE","GAMMA","DTE",mn,mx,false,&sl,&dl); break;
    }
    case ChartType::VegaSurface: {
        minmax(vega_data_.z,mn,mx);
        auto sl=fmt_strikes(vega_data_.strikes), dl=fmt_dtes(vega_data_.expirations);
        surface_3d_->set_surface(vega_data_.z,"STRIKE","VEGA","DTE",mn,mx,false,&sl,&dl); break;
    }
    case ChartType::ThetaSurface: {
        minmax(theta_data_.z,mn,mx);
        auto sl=fmt_strikes(theta_data_.strikes), dl=fmt_dtes(theta_data_.expirations);
        surface_3d_->set_surface(theta_data_.z,"STRIKE","THETA","DTE",mn,mx,true,&sl,&dl); break;
    }
    case ChartType::SkewSurface: {
        minmax(skew_data_.z,mn,mx);
        std::vector<std::string> dl,sl;
        for(float d:skew_data_.deltas){char b[16];std::snprintf(b,16,"%dD",(int)d);dl.push_back(b);}
        for(int e:skew_data_.expirations){char b[16];std::snprintf(b,16,"%dD",e);sl.push_back(b);}
        surface_3d_->set_surface(skew_data_.z,"DELTA","SKEW %","DTE",mn,mx,true,&dl,&sl); break;
    }
    case ChartType::LocalVolSurface: {
        minmax(local_vol_data_.z,mn,mx);
        auto sl=fmt_strikes(local_vol_data_.strikes), dl=fmt_dtes(local_vol_data_.expirations);
        surface_3d_->set_surface(local_vol_data_.z,"STRIKE","LV %","DTE",mn,mx,false,&sl,&dl); break;
    }
    case ChartType::YieldCurve: {
        minmax(yield_data_.z,mn,mx);
        std::vector<std::string> ml;
        for(int m:yield_data_.maturities){char b[16];std::snprintf(b,16,"%dM",m);ml.push_back(b);}
        surface_3d_->set_surface(yield_data_.z,"MATURITY","YIELD %","TIME",mn,mx,false,&ml,nullptr); break;
    }
    case ChartType::SwaptionVol: {
        minmax(swaption_data_.z,mn,mx);
        auto tl=fmt_months(swaption_data_.swap_tenors), el=fmt_months(swaption_data_.option_expiries);
        surface_3d_->set_surface(swaption_data_.z,"TENOR","VOL bp","EXPIRY",mn,mx,false,&tl,&el); break;
    }
    case ChartType::BondSpread: {
        minmax(bond_spread_data_.z,mn,mx);
        auto ml=fmt_months(bond_spread_data_.maturities);
        surface_3d_->set_surface(bond_spread_data_.z,"MATURITY","SPREAD bp","RATING",mn,mx,false,&ml,nullptr); break;
    }
    case ChartType::FXVol: {
        minmax(fx_vol_data_.z,mn,mx);
        std::vector<std::string> dl,tl;
        for(float d:fx_vol_data_.deltas){char b[16];std::snprintf(b,16,"%dD",(int)d);dl.push_back(b);}
        for(int t:fx_vol_data_.tenors){char b[16];std::snprintf(b,16,"%dD",t);tl.push_back(b);}
        surface_3d_->set_surface(fx_vol_data_.z,"TENOR","VOL %","DELTA",mn,mx,false,&dl,&tl); break;
    }
    case ChartType::CDSSpread: {
        minmax(cds_data_.z,mn,mx);
        auto tl=fmt_months(cds_data_.tenors);
        surface_3d_->set_surface(cds_data_.z,"TENOR","SPREAD bp","ENTITY",mn,mx,false,&tl,nullptr); break;
    }
    case ChartType::CreditTransition: {
        minmax(credit_trans_data_.z,mn,mx);
        surface_3d_->set_surface(credit_trans_data_.z,"TO RATING","PROB %","FROM RATING",mn,mx,false); break;
    }
    case ChartType::Correlation: {
        // Use last time-slice as a square matrix
        int n = (int)corr_data_.assets.size();
        if (!corr_data_.z.empty()) {
            std::vector<std::vector<float>> mat(n, std::vector<float>(n));
            const auto& last = corr_data_.z.back();
            for(int i=0;i<n;i++) for(int j=0;j<n;j++) mat[i][j]=last[i*n+j];
            surface_3d_->set_surface(mat,"ASSET j","CORR","ASSET i",-1.0f,1.0f,true);
        }
        break;
    }
    case ChartType::PCA: {
        minmax(pca_data_.z,mn,mx);
        surface_3d_->set_surface(pca_data_.z,"FACTOR","LOADING","ASSET",mn,mx,true); break;
    }
    case ChartType::VaR: {
        minmax(var_data_.z,mn,mx);
        std::vector<std::string> hl2;
        for(int h:var_data_.horizons){char b[16];std::snprintf(b,16,"%dD",h);hl2.push_back(b);}
        surface_3d_->set_surface(var_data_.z,"HORIZON","VaR %","CONFIDENCE",mn,mx,false,&hl2,nullptr); break;
    }
    case ChartType::StressTestPnL: {
        minmax(stress_data_.z,mn,mx);
        surface_3d_->set_surface(stress_data_.z,"PORTFOLIO","P&L %","SCENARIO",mn,mx,true); break;
    }
    case ChartType::FactorExposure: {
        minmax(factor_data_.z,mn,mx);
        surface_3d_->set_surface(factor_data_.z,"FACTOR","EXPOSURE","ASSET",mn,mx,true); break;
    }
    case ChartType::LiquidityHeatmap: {
        minmax(liquidity_data_.z,mn,mx);
        auto sl=fmt_strikes(liquidity_data_.strikes), dl=fmt_dtes(liquidity_data_.expirations);
        surface_3d_->set_surface(liquidity_data_.z,"STRIKE","SPREAD","DTE",mn,mx,false,&sl,&dl); break;
    }
    case ChartType::Drawdown: {
        minmax(drawdown_data_.z,mn,mx);
        std::vector<std::string> wl;
        for(int w:drawdown_data_.windows){char b[16];std::snprintf(b,16,"%dD",w);wl.push_back(b);}
        surface_3d_->set_surface(drawdown_data_.z,"WINDOW","DRAWDOWN %","ASSET",mn,mx,false,&wl,nullptr); break;
    }
    case ChartType::BetaSurface: {
        minmax(beta_data_.z,mn,mx);
        std::vector<std::string> hl2;
        for(int h:beta_data_.horizons){char b[16];std::snprintf(b,16,"%dD",h);hl2.push_back(b);}
        surface_3d_->set_surface(beta_data_.z,"HORIZON","BETA","ASSET",mn,mx,true,&hl2,nullptr); break;
    }
    case ChartType::ImpliedDividend: {
        minmax(impl_div_data_.z,mn,mx);
        auto sl=fmt_strikes(impl_div_data_.strikes), dl=fmt_dtes(impl_div_data_.expirations);
        surface_3d_->set_surface(impl_div_data_.z,"STRIKE","DIV YIELD %","DTE",mn,mx,false,&sl,&dl); break;
    }
    case ChartType::CommodityForward: {
        minmax(cmdty_fwd_data_.z,mn,mx);
        std::vector<std::string> ml;
        for(int m:cmdty_fwd_data_.contract_months){char b[16];std::snprintf(b,16,"M%d",m);ml.push_back(b);}
        surface_3d_->set_surface(cmdty_fwd_data_.z,"CONTRACT","PRICE","COMMODITY",mn,mx,false,&ml,nullptr); break;
    }
    case ChartType::CommodityVol: {
        minmax(cmdty_vol_data_.z,mn,mx);
        auto sl=fmt_strikes(cmdty_vol_data_.strikes), dl=fmt_dtes(cmdty_vol_data_.expirations);
        surface_3d_->set_surface(cmdty_vol_data_.z,"STRIKE","VOL %","DTE",mn,mx,false,&sl,&dl); break;
    }
    case ChartType::CrackSpread: {
        minmax(crack_data_.z,mn,mx);
        std::vector<std::string> ml;
        for(int m:crack_data_.contract_months){char b[16];std::snprintf(b,16,"M%d",m);ml.push_back(b);}
        surface_3d_->set_surface(crack_data_.z,"CONTRACT","SPREAD","TYPE",mn,mx,true,&ml,nullptr); break;
    }
    case ChartType::ContangoBackwardation: {
        minmax(contango_data_.z,mn,mx);
        std::vector<std::string> ml;
        for(int m:contango_data_.contract_months){char b[16];std::snprintf(b,16,"M%d",m);ml.push_back(b);}
        surface_3d_->set_surface(contango_data_.z,"CONTRACT","% FROM SPOT","COMMODITY",mn,mx,true,&ml,nullptr); break;
    }
    case ChartType::OISBasis: {
        minmax(ois_data_.z,mn,mx);
        auto tl=fmt_months(ois_data_.tenors);
        surface_3d_->set_surface(ois_data_.z,"TENOR","BASIS bp","TIME",mn,mx,true,&tl,nullptr); break;
    }
    case ChartType::RealYield: {
        minmax(real_yield_data_.z,mn,mx);
        auto ml=fmt_months(real_yield_data_.maturities);
        surface_3d_->set_surface(real_yield_data_.z,"MATURITY","REAL YIELD %","TIME",mn,mx,false,&ml,nullptr); break;
    }
    case ChartType::ForwardRate: {
        minmax(fwd_rate_data_.z,mn,mx);
        auto pl=fmt_months(fwd_rate_data_.forward_periods);
        surface_3d_->set_surface(fwd_rate_data_.z,"FWD PERIOD","RATE %","START",mn,mx,false,&pl,nullptr); break;
    }
    case ChartType::FXForwardPoints: {
        minmax(fx_fwd_data_.z,mn,mx);
        auto tl=fmt_months(fx_fwd_data_.tenors);
        surface_3d_->set_surface(fx_fwd_data_.z,"TENOR","FWD PTS","PAIR",mn,mx,true,&tl,nullptr); break;
    }
    case ChartType::CrossCurrencyBasis: {
        minmax(xccy_data_.z,mn,mx);
        auto tl=fmt_months(xccy_data_.tenors);
        surface_3d_->set_surface(xccy_data_.z,"TENOR","BASIS bp","PAIR",mn,mx,true,&tl,nullptr); break;
    }
    case ChartType::RecoveryRate: {
        minmax(recovery_data_.z,mn,mx);
        surface_3d_->set_surface(recovery_data_.z,"SENIORITY","RECOVERY %","SECTOR",mn,mx,false); break;
    }
    case ChartType::CapFloorVol: {
        minmax(capfloor_data_.z,mn,mx);
        std::vector<std::string> sl2;
        for(float s:capfloor_data_.strikes){char b[16];std::snprintf(b,16,"%.1f%%",s);sl2.push_back(b);}
        auto ml2=fmt_months(capfloor_data_.maturities);
        surface_3d_->set_surface(capfloor_data_.z,"STRIKE","VOL bp","MATURITY",mn,mx,false,&sl2,&ml2); break;
    }
    case ChartType::InflationExpectations: {
        minmax(inflation_data_.z,mn,mx);
        std::vector<std::string> hl2;
        for(int h:inflation_data_.horizons){char b[16];std::snprintf(b,16,"%dY",h);hl2.push_back(b);}
        surface_3d_->set_surface(inflation_data_.z,"HORIZON","BREAKEVEN %","TIME",mn,mx,false,&hl2,nullptr); break;
    }
    case ChartType::MonetaryPolicyPath: {
        minmax(monetary_data_.z,mn,mx);
        std::vector<std::string> ml;
        for(int m:monetary_data_.meetings_ahead){char b[16];std::snprintf(b,16,"Mtg%d",m);ml.push_back(b);}
        surface_3d_->set_surface(monetary_data_.z,"MEETING","RATE %","CENTRAL BANK",mn,mx,false,&ml,nullptr); break;
    }
    default:
        surface_3d_->clear();
        break;
    }
}

void SurfaceAnalyticsScreen::update_metrics() {
    metrics_panel_->update_metrics(active_chart_,
        vol_data_, delta_data_, gamma_data_, vega_data_, theta_data_,
        skew_data_, local_vol_data_, yield_data_, swaption_data_, capfloor_data_,
        bond_spread_data_, ois_data_, real_yield_data_, fwd_rate_data_,
        fx_vol_data_, fx_fwd_data_, xccy_data_, cds_data_, credit_trans_data_,
        recovery_data_, cmdty_fwd_data_, cmdty_vol_data_, crack_data_, contango_data_,
        corr_data_, pca_data_, var_data_, stress_data_, factor_data_,
        liquidity_data_, drawdown_data_, beta_data_, impl_div_data_,
        inflation_data_, monetary_data_);
}

// ============================================================================
// Slot handlers
void SurfaceAnalyticsScreen::on_category_clicked(int index) {
    if (index == active_category_) return;
    active_category_ = index;
    const auto cats = get_surface_categories();
    active_chart_ = cats[index].types[0];

    // Rebuild category bar to update active styling
    auto* main_layout = qobject_cast<QVBoxLayout*>(layout());
    if (main_layout) {
        int idx = main_layout->indexOf(category_bar_);
        main_layout->removeWidget(category_bar_);
        category_bar_->deleteLater();
        category_bar_ = build_category_bar();
        main_layout->insertWidget(idx, category_bar_);
    }
    refresh_surface_bar();
    databento_panel_->set_active_chart(active_chart_,
        VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_surface_clicked(int /*cat*/, int surf_index) {
    const auto cats = get_surface_categories();
    if (active_category_ >= (int)cats.size()) return;
    const auto& cat = cats[active_category_];
    if (surf_index >= (int)cat.types.size()) return;
    active_chart_ = cat.types[surf_index];
    refresh_surface_bar();
    databento_panel_->set_active_chart(active_chart_,
        VOL_SYMBOLS[selected_symbol_], VOL_SPOTS[selected_symbol_]);
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_view_3d() {
    show_table_ = false;
    btn_3d_->setChecked(true);
    btn_table_->setChecked(false);
    update_chart();
}

void SurfaceAnalyticsScreen::on_view_table() {
    show_table_ = true;
    btn_3d_->setChecked(false);
    btn_table_->setChecked(true);
    update_chart();
}

void SurfaceAnalyticsScreen::on_refresh() {
    load_demo_data();
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_symbol_changed(int index) {
    if (index == selected_symbol_) return;
    selected_symbol_ = index;
    load_demo_data();
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_import_csv() {
    QString path = QFileDialog::getOpenFileName(this, "Import CSV", "",
        "CSV Files (*.csv);;All Files (*)");
    if (path.isEmpty()) return;
    dispatch_csv(path);
}

void SurfaceAnalyticsScreen::dispatch_csv(const QString& path) {
    std::string err;
    auto rows = parse_csv_file(path, err);
    if (rows.empty()) {
        QMessageBox::warning(this, "Import Error", QString::fromStdString(err));
        return;
    }

    bool ok = false;
    switch (active_chart_) {
    case ChartType::Volatility:     ok=load_vol_surface(rows,vol_data_,err); break;
    case ChartType::DeltaSurface:   ok=load_greeks_surface(rows,delta_data_,err,"Delta"); break;
    case ChartType::GammaSurface:   ok=load_greeks_surface(rows,gamma_data_,err,"Gamma"); break;
    case ChartType::VegaSurface:    ok=load_greeks_surface(rows,vega_data_,err,"Vega"); break;
    case ChartType::ThetaSurface:   ok=load_greeks_surface(rows,theta_data_,err,"Theta"); break;
    case ChartType::SkewSurface:    ok=load_skew_surface(rows,skew_data_,err); break;
    case ChartType::LocalVolSurface:ok=load_local_vol(rows,local_vol_data_,err); break;
    case ChartType::YieldCurve:     ok=load_yield_curve(rows,yield_data_,err); break;
    case ChartType::SwaptionVol:    ok=load_swaption_vol(rows,swaption_data_,err); break;
    case ChartType::CapFloorVol:    ok=load_capfloor_vol(rows,capfloor_data_,err); break;
    case ChartType::BondSpread:     ok=load_bond_spread(rows,bond_spread_data_,err); break;
    case ChartType::OISBasis:       ok=load_ois_basis(rows,ois_data_,err); break;
    case ChartType::RealYield:      ok=load_real_yield(rows,real_yield_data_,err); break;
    case ChartType::ForwardRate:    ok=load_forward_rate(rows,fwd_rate_data_,err); break;
    case ChartType::FXVol:          ok=load_fx_vol(rows,fx_vol_data_,err); break;
    case ChartType::FXForwardPoints:ok=load_fx_forward(rows,fx_fwd_data_,err); break;
    case ChartType::CrossCurrencyBasis: ok=load_xccy_basis(rows,xccy_data_,err); break;
    case ChartType::CDSSpread:      ok=load_cds_spread(rows,cds_data_,err); break;
    case ChartType::CreditTransition:ok=load_credit_trans(rows,credit_trans_data_,err); break;
    case ChartType::RecoveryRate:   ok=load_recovery_rate(rows,recovery_data_,err); break;
    case ChartType::CommodityForward:ok=load_cmdty_forward(rows,cmdty_fwd_data_,err); break;
    case ChartType::CommodityVol:   ok=load_cmdty_vol(rows,cmdty_vol_data_,err); break;
    case ChartType::CrackSpread:    ok=load_crack_spread(rows,crack_data_,err); break;
    case ChartType::ContangoBackwardation:ok=load_contango(rows,contango_data_,err); break;
    case ChartType::Correlation:    ok=load_correlation(rows,corr_data_,err); break;
    case ChartType::PCA:            ok=load_pca(rows,pca_data_,err); break;
    case ChartType::VaR:            ok=load_var(rows,var_data_,err); break;
    case ChartType::StressTestPnL:  ok=load_stress_test(rows,stress_data_,err); break;
    case ChartType::FactorExposure: ok=load_factor_exposure(rows,factor_data_,err); break;
    case ChartType::LiquidityHeatmap:ok=load_liquidity(rows,liquidity_data_,err); break;
    case ChartType::Drawdown:       ok=load_drawdown(rows,drawdown_data_,err); break;
    case ChartType::BetaSurface:    ok=load_beta(rows,beta_data_,err); break;
    case ChartType::ImpliedDividend:ok=load_implied_div(rows,impl_div_data_,err); break;
    case ChartType::InflationExpectations:ok=load_inflation(rows,inflation_data_,err); break;
    case ChartType::MonetaryPolicyPath:ok=load_monetary(rows,monetary_data_,err); break;
    default: err="Surface not supported for CSV import"; break;
    }

    if (!ok) {
        QMessageBox::warning(this, "Import Error", QString::fromStdString(err));
    } else {
        update_chart();
        update_metrics();
    }
}

// ── P3: connect Databento panel signals on show, disconnect on hide ───────────
void SurfaceAnalyticsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    connect(databento_panel_, &SurfaceDatabentoPanel::vol_surface_received,
            this, &SurfaceAnalyticsScreen::on_vol_surface_received, Qt::UniqueConnection);
    connect(databento_panel_, &SurfaceDatabentoPanel::ohlcv_received,
            this, &SurfaceAnalyticsScreen::on_ohlcv_received, Qt::UniqueConnection);
    connect(databento_panel_, &SurfaceDatabentoPanel::futures_received,
            this, &SurfaceAnalyticsScreen::on_futures_received, Qt::UniqueConnection);
}

void SurfaceAnalyticsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    disconnect(databento_panel_, nullptr, this, nullptr);
}

// ── Databento data slots ──────────────────────────────────────────────────────
void SurfaceAnalyticsScreen::on_vol_surface_received(const fincept::DatabentoVolSurfaceResult& r) {
    if (!r.success) return;
    if (!r.vol.z.empty())   vol_data_       = r.vol;
    if (!r.delta.z.empty()) delta_data_     = r.delta;
    if (!r.gamma.z.empty()) gamma_data_     = r.gamma;
    if (!r.vega.z.empty())  vega_data_      = r.vega;
    if (!r.theta.z.empty()) theta_data_     = r.theta;
    if (!r.skew.z.empty())  skew_data_      = r.skew;
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_ohlcv_received(const fincept::DatabentoOhlcvResult& r) {
    if (!r.success || r.data.isEmpty()) return;
    // Rebuild correlation, PCA, drawdown, beta from OHLCV data
    // Convert QHash to std::vector<std::string> asset list for demo generators
    // (actual computation uses the demo data regenerated with live symbol names)
    std::vector<std::string> assets;
    for (const QString& sym : r.data.keys())
        assets.push_back(sym.toStdString());
    if (assets.empty()) return;
    corr_assets_   = assets;
    corr_data_     = generate_correlation(assets);
    pca_data_      = generate_pca(assets);
    drawdown_data_ = generate_drawdown(assets);
    beta_data_     = generate_beta(assets);
    factor_data_   = generate_factor_exposure(assets);
    update_chart();
    update_metrics();
}

void SurfaceAnalyticsScreen::on_futures_received(const fincept::DatabentoFuturesResult& r) {
    if (!r.success) return;
    if (!r.forward.z.empty())  cmdty_fwd_data_ = r.forward;
    if (!r.contango.z.empty()) contango_data_  = r.contango;
    update_chart();
    update_metrics();
}

} // namespace fincept::surface
