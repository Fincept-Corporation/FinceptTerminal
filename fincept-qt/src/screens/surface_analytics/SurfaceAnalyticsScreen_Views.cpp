// src/screens/surface_analytics/SurfaceAnalyticsScreen_Views.cpp
//
// View-mode renderers: update_chart (3D surface), update_metrics, the data
// inspector lineage panel, the line-view variant, plus the canned demo
// dataset loader.
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
    QString qsym = current_symbol_or_default();
    std::string sym = qsym.toStdString();
    float spot = spot_for(qsym);

    // Build a basket vector<string> from the control-panel state for risk surfaces.
    std::vector<std::string> basket;
    if (control_panel_) {
        for (const QString& s : control_panel_->state().basket)
            basket.push_back(s.toStdString());
    }
    if (basket.empty()) {
        for (auto* s : defaults::RISK_BASKET)
            basket.emplace_back(s);
    }

    vol_data_ = generate_vol_surface(sym.c_str(), spot);
    delta_data_ = generate_delta_surface(sym.c_str(), spot);
    gamma_data_ = generate_gamma_surface(sym.c_str(), spot);
    vega_data_ = generate_vega_surface(sym.c_str(), spot);
    theta_data_ = generate_theta_surface(sym.c_str(), spot);
    skew_data_ = generate_skew_surface(sym.c_str());
    local_vol_data_ = generate_local_vol(sym.c_str(), spot);

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

    corr_data_ = generate_correlation(basket);
    pca_data_ = generate_pca(basket);
    var_data_ = generate_var();
    stress_data_ = generate_stress_test();
    factor_data_ = generate_factor_exposure(basket);
    liquidity_data_ = generate_liquidity(sym.c_str(), spot);
    drawdown_data_ = generate_drawdown(basket);
    beta_data_ = generate_beta(basket);
    impl_div_data_ = generate_implied_dividend(sym.c_str(), spot);

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

    // ── Capability-driven view-mode coercion ───────────────────────────────
    // A surface declares which views it supports. If the active view_mode_ is
    // not supported, fall back to the first one that is.
    const auto& cap = capability_for(active_chart_);
    ViewMode mode = view_mode_;
    if (mode == ViewMode::Surface3D && !cap.supports_3d)
        mode = cap.supports_table ? ViewMode::Table : ViewMode::Line;
    if (mode == ViewMode::Line && !cap.supports_line)
        mode = cap.supports_table ? ViewMode::Table : ViewMode::Surface3D;
    if (mode == ViewMode::Table && !cap.supports_table)
        mode = cap.supports_3d ? ViewMode::Surface3D : ViewMode::Line;

    surface_3d_->set_supported(cap.supports_3d);

    if (mode == ViewMode::Line) {
        view_stack_->setCurrentIndex(2);
        update_line_view();
        return;
    }

    if (mode == ViewMode::Table) {
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

// ── Metrics routing — now hands the active surface's z grid to the control panel
// which computes summary stats locally (count/min/max/mean/median/std/skew/kurt).
void SurfaceAnalyticsScreen::update_metrics() {
    if (!control_panel_)
        return;
    if (const auto* z = active_z_grid())
        control_panel_->update_metrics(*z);
    else
        control_panel_->update_metrics({});
}

const std::vector<std::vector<float>>* SurfaceAnalyticsScreen::active_z_grid() const {
    switch (active_chart_) {
        case ChartType::Volatility: return &vol_data_.z;
        case ChartType::DeltaSurface: return &delta_data_.z;
        case ChartType::GammaSurface: return &gamma_data_.z;
        case ChartType::VegaSurface: return &vega_data_.z;
        case ChartType::ThetaSurface: return &theta_data_.z;
        case ChartType::SkewSurface: return &skew_data_.z;
        case ChartType::LocalVolSurface: return &local_vol_data_.z;
        case ChartType::YieldCurve: return &yield_data_.z;
        case ChartType::SwaptionVol: return &swaption_data_.z;
        case ChartType::CapFloorVol: return &capfloor_data_.z;
        case ChartType::BondSpread: return &bond_spread_data_.z;
        case ChartType::OISBasis: return &ois_data_.z;
        case ChartType::RealYield: return &real_yield_data_.z;
        case ChartType::ForwardRate: return &fwd_rate_data_.z;
        case ChartType::FXVol: return &fx_vol_data_.z;
        case ChartType::FXForwardPoints: return &fx_fwd_data_.z;
        case ChartType::CrossCurrencyBasis: return &xccy_data_.z;
        case ChartType::CDSSpread: return &cds_data_.z;
        case ChartType::CreditTransition: return &credit_trans_data_.z;
        case ChartType::RecoveryRate: return &recovery_data_.z;
        case ChartType::CommodityForward: return &cmdty_fwd_data_.z;
        case ChartType::CommodityVol: return &cmdty_vol_data_.z;
        case ChartType::CrackSpread: return &crack_data_.z;
        case ChartType::ContangoBackwardation: return &contango_data_.z;
        case ChartType::Correlation: return &corr_data_.z;
        case ChartType::PCA: return &pca_data_.z;
        case ChartType::VaR: return &var_data_.z;
        case ChartType::StressTestPnL: return &stress_data_.z;
        case ChartType::FactorExposure: return &factor_data_.z;
        case ChartType::LiquidityHeatmap: return &liquidity_data_.z;
        case ChartType::Drawdown: return &drawdown_data_.z;
        case ChartType::BetaSurface: return &beta_data_.z;
        case ChartType::ImpliedDividend: return &impl_div_data_.z;
        case ChartType::InflationExpectations: return &inflation_data_.z;
        case ChartType::MonetaryPolicyPath: return &monetary_data_.z;
    }
    return nullptr;
}

void SurfaceAnalyticsScreen::update_inspector_lineage() {
    if (!data_inspector_)
        return;
    const auto& cap = capability_for(active_chart_);
    QString date_range;
    if (control_panel_) {
        const auto& s = control_panel_->state();
        if (s.start_date.isValid() && s.end_date.isValid())
            date_range = QString("%1 → %2")
                             .arg(s.start_date.toString("yyyy-MM-dd"))
                             .arg(s.end_date.toString("yyyy-MM-dd"));
    }
    QString sym = current_symbol_or_default();
    if (cap.tier == SurfaceTier::EQUITIES && control_panel_)
        sym = control_panel_->state().basket.join(",");
    qint64 count = 0;
    if (const auto* z = active_z_grid())
        for (const auto& row : *z)
            count += (qint64)row.size();
    data_inspector_->set_lineage(QString::fromUtf8(cap.dataset),
                                 QString::fromUtf8(cap.schema),
                                 QString::fromUtf8(cap.symbology),
                                 sym, date_range, count, 0.0);

    // Fire-and-forget cost lookup. Skip for DEMO (no dataset) and for
    // capabilities whose schema is a composite ("definition+cbbo-1s") since
    // metadata.get_cost takes a single schema. Use the first listed.
    if (cap.tier == SurfaceTier::DEMO || !control_panel_)
        return;
    auto& svc = DatabentoService::instance();
    if (!svc.has_api_key())
        return;
    const auto& s = control_panel_->state();
    if (!s.start_date.isValid() || !s.end_date.isValid())
        return;
    QString schema = QString::fromUtf8(cap.schema);
    int plus = schema.indexOf('+');
    if (plus >= 0)
        schema = schema.left(plus);
    DbCostQuery q;
    q.dataset = QString::fromUtf8(cap.dataset);
    q.schema = schema;
    q.start = s.start_date;
    q.end = s.end_date;
    q.stype_in = QString::fromUtf8(cap.symbology);
    if (cap.tier == SurfaceTier::EQUITIES)
        q.symbols = s.basket.isEmpty() ? QStringList{s.symbol} : s.basket;
    else if (QString::fromUtf8(cap.symbology) == "parent")
        q.symbols = QStringList{s.symbol + ".OPT"};
    else
        q.symbols = QStringList{s.symbol};

    QPointer<SurfaceAnalyticsScreen> self = this;
    QString ds = q.dataset;
    QString sch = QString::fromUtf8(cap.schema);
    QString symb = QString::fromUtf8(cap.symbology);
    QString sym_text = sym;
    QString dr = date_range;
    qint64 row_ct = count;
    svc.get_cost(q, [self, ds, sch, symb, sym_text, dr, row_ct](DbCostResult r) {
        if (!self || !self->data_inspector_)
            return;
        if (!r.success)
            return;
        self->data_inspector_->set_lineage(ds, sch, symb, sym_text, dr,
                                           row_ct > 0 ? row_ct : r.record_count,
                                           r.cost_usd);
    });
}

void SurfaceAnalyticsScreen::update_line_view() {
    if (!surface_line_)
        return;
    auto fmt_months_str = [](const std::vector<int>& v) {
        QStringList out;
        for (int i : v) out << QString("%1M").arg(i);
        return out;
    };

    switch (active_chart_) {
        case ChartType::YieldCurve: {
            // Show the latest column of the yield matrix as a single curve
            if (yield_data_.z.empty() || yield_data_.maturities.empty())
                break;
            std::vector<float> xs, ys;
            for (size_t i = 0; i < yield_data_.maturities.size(); ++i) {
                xs.push_back((float)yield_data_.maturities[i]);
                ys.push_back(yield_data_.z[0].size() > i ? yield_data_.z[0][i] : 0.0f);
            }
            surface_line_->set_curve("YIELD CURVE", xs, ys, fmt_months_str(yield_data_.maturities),
                                     "Maturity (months)", "Yield %", QColor(63, 185, 80));
            return;
        }
        case ChartType::ContangoBackwardation: {
            std::vector<SurfaceLineWidget::Series> series;
            for (size_t i = 0; i < contango_data_.commodities.size() && i < contango_data_.z.size(); ++i) {
                SurfaceLineWidget::Series s;
                s.name = QString::fromStdString(contango_data_.commodities[i]);
                for (size_t k = 0; k < contango_data_.contract_months.size(); ++k)
                    s.x_values.push_back((float)contango_data_.contract_months[k]);
                s.y_values.assign(contango_data_.z[i].begin(), contango_data_.z[i].end());
                static const QColor palette[] = {
                    QColor(217, 119, 6), QColor(88, 166, 255), QColor(63, 185, 80),
                    QColor(220, 80, 80), QColor(155, 114, 255), QColor(89, 196, 217)};
                s.color = palette[i % 6];
                series.push_back(s);
            }
            surface_line_->set_series("CONTANGO / BACKWARDATION", series, "Contract month", "Roll %");
            return;
        }
        case ChartType::CommodityForward: {
            std::vector<SurfaceLineWidget::Series> series;
            for (size_t i = 0; i < cmdty_fwd_data_.commodities.size() && i < cmdty_fwd_data_.z.size(); ++i) {
                SurfaceLineWidget::Series s;
                s.name = QString::fromStdString(cmdty_fwd_data_.commodities[i]);
                for (size_t k = 0; k < cmdty_fwd_data_.contract_months.size(); ++k)
                    s.x_values.push_back((float)cmdty_fwd_data_.contract_months[k]);
                s.y_values.assign(cmdty_fwd_data_.z[i].begin(), cmdty_fwd_data_.z[i].end());
                static const QColor palette[] = {
                    QColor(217, 119, 6), QColor(88, 166, 255), QColor(63, 185, 80),
                    QColor(220, 80, 80), QColor(155, 114, 255), QColor(89, 196, 217)};
                s.color = palette[i % 6];
                series.push_back(s);
            }
            surface_line_->set_series("FUTURES TERM STRUCTURE", series, "Contract month", "Price");
            return;
        }
        case ChartType::CrackSpread: {
            std::vector<SurfaceLineWidget::Series> series;
            for (size_t i = 0; i < crack_data_.spread_types.size() && i < crack_data_.z.size(); ++i) {
                SurfaceLineWidget::Series s;
                s.name = QString::fromStdString(crack_data_.spread_types[i]);
                for (size_t k = 0; k < crack_data_.contract_months.size(); ++k)
                    s.x_values.push_back((float)crack_data_.contract_months[k]);
                s.y_values.assign(crack_data_.z[i].begin(), crack_data_.z[i].end());
                static const QColor palette[] = {QColor(217, 119, 6), QColor(88, 166, 255),
                                                 QColor(63, 185, 80), QColor(220, 80, 80)};
                s.color = palette[i % 4];
                series.push_back(s);
            }
            surface_line_->set_series("CRACK / CRUSH SPREAD", series, "Contract month", "Spread $/bbl");
            return;
        }
        case ChartType::FXForwardPoints: {
            std::vector<SurfaceLineWidget::Series> series;
            for (size_t i = 0; i < fx_fwd_data_.pairs.size() && i < fx_fwd_data_.z.size(); ++i) {
                SurfaceLineWidget::Series s;
                s.name = QString::fromStdString(fx_fwd_data_.pairs[i]);
                for (size_t k = 0; k < fx_fwd_data_.tenors.size(); ++k)
                    s.x_values.push_back((float)fx_fwd_data_.tenors[k]);
                s.y_values.assign(fx_fwd_data_.z[i].begin(), fx_fwd_data_.z[i].end());
                static const QColor palette[] = {QColor(217, 119, 6), QColor(88, 166, 255),
                                                 QColor(63, 185, 80), QColor(220, 80, 80)};
                s.color = palette[i % 4];
                series.push_back(s);
            }
            surface_line_->set_series("FX FORWARD POINTS", series, "Tenor (months)", "Fwd points");
            return;
        }
        case ChartType::InflationExpectations: {
            if (inflation_data_.z.empty() || inflation_data_.horizons.empty())
                break;
            std::vector<float> xs, ys;
            for (size_t i = 0; i < inflation_data_.horizons.size(); ++i) {
                xs.push_back((float)inflation_data_.horizons[i]);
                ys.push_back(inflation_data_.z[0].size() > i ? inflation_data_.z[0][i] : 0.0f);
            }
            QStringList xl;
            for (int h : inflation_data_.horizons) xl << QString("%1Y").arg(h);
            surface_line_->set_curve("INFLATION EXPECTATIONS", xs, ys, xl,
                                     "Horizon (years)", "Breakeven %",
                                     QColor(89, 196, 217));
            return;
        }
        case ChartType::MonetaryPolicyPath: {
            std::vector<SurfaceLineWidget::Series> series;
            for (size_t i = 0; i < monetary_data_.central_banks.size() && i < monetary_data_.z.size(); ++i) {
                SurfaceLineWidget::Series s;
                s.name = QString::fromStdString(monetary_data_.central_banks[i]);
                for (size_t k = 0; k < monetary_data_.meetings_ahead.size(); ++k)
                    s.x_values.push_back((float)monetary_data_.meetings_ahead[k]);
                s.y_values.assign(monetary_data_.z[i].begin(), monetary_data_.z[i].end());
                static const QColor palette[] = {QColor(217, 119, 6), QColor(88, 166, 255),
                                                 QColor(63, 185, 80), QColor(220, 80, 80)};
                s.color = palette[i % 4];
                series.push_back(s);
            }
            surface_line_->set_series("RATE PATH", series, "Meetings ahead", "Rate %");
            return;
        }
        default:
            break;
    }
    // Fallback — clear if the current chart has no line representation.
    surface_line_->clear();
}

} // namespace fincept::surface
