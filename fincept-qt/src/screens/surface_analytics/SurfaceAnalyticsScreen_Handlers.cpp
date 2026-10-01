// src/screens/surface_analytics/SurfaceAnalyticsScreen_Handlers.cpp
//
// User-interaction handlers: category/surface selection, view-mode toggles,
// CSV import, refresh, and the symbol/fetch flow that drives Databento
// requests via the control panel.
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
#include "core/symbol/SymbolContext.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QFileInfo>
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

    if (control_panel_) {
        // Badge first: set_capability renders the tier, and it must render DEMO
        // when this surface has never been fetched.
        sync_synthetic_badge();
        control_panel_->set_capability(active_chart_);
    }
    load_dataset_range_for_active_capability();
    update_chart();
    update_metrics();
    update_inspector_lineage();
    fincept::ScreenStateManager::instance().notify_changed(this);
}

void SurfaceAnalyticsScreen::on_surface_clicked(int cat, int surf_index) {
    const auto cats = get_surface_categories();
    if (cat < (int)cats.size() && surf_index < (int)cats[cat].types.size())
        active_chart_ = cats[cat].types[surf_index];
    refresh_surface_bar();
    if (control_panel_) {
        sync_synthetic_badge();
        control_panel_->set_capability(active_chart_);
    }
    load_dataset_range_for_active_capability();
    update_chart();
    update_metrics();
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::on_view_3d() {
    view_mode_ = ViewMode::Surface3D;
    apply_view_mode_buttons();
    update_chart();
}

void SurfaceAnalyticsScreen::on_view_table() {
    view_mode_ = ViewMode::Table;
    apply_view_mode_buttons();
    update_chart();
}

void SurfaceAnalyticsScreen::on_view_line() {
    view_mode_ = ViewMode::Line;
    apply_view_mode_buttons();
    update_chart();
}

void SurfaceAnalyticsScreen::on_import_csv() {
    QString path = QFileDialog::getOpenFileName(this, tr("Import Surface CSV"), {}, tr("CSV Files (*.csv)"));
    if (!path.isEmpty())
        dispatch_csv(path);
}

void SurfaceAnalyticsScreen::on_refresh() {
    // REFRESH used to *always* re-roll SurfaceDemoData's rand() output, so on a
    // Databento-backed surface it silently replaced real fetched data with new
    // fabricated numbers under an unchanged badge. Route it to a real re-fetch
    // whenever one is possible; only fall back to regenerating sample data for
    // surfaces that genuinely have no source, and say so.
    const auto& cap = capability_for(active_chart_);
    const bool fetchable = cap.tier != SurfaceTier::DEMO && DatabentoService::instance().has_api_key();
    if (fetchable) {
        on_fetch_requested();
        return;
    }
    load_demo_data();
    update_chart();
    update_metrics();
    update_inspector_lineage();
    if (data_inspector_) {
        data_inspector_->set_status(cap.tier == SurfaceTier::DEMO
                                        ? tr("Regenerated synthetic sample data — this surface has no live source.")
                                        : tr("Regenerated synthetic sample data — no Databento API key configured."),
                                    false);
    }
}

void SurfaceAnalyticsScreen::on_controls_changed() {
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::on_control_symbol_changed(const QString& sym) {
    // Follow the new underlying's quote, then rebuild the sample surfaces around it so
    // the chart isn't stale.
    resubscribe_spot();
    load_demo_data();
    update_chart();
    update_metrics();
    update_inspector_lineage();

    // A user-typed OPRA underlying drives the linked group (a symbol applied FROM the
    // group is not echoed back). Only for option-underlying surfaces: the other
    // surfaces take a commodity root or a basket, which isn't an equity ticker.
    if (!applying_group_symbol_ && link_group_ != fincept::SymbolGroup::None && !sym.isEmpty() &&
        QString::fromUtf8(capability_for(active_chart_).dataset) == QLatin1String("OPRA.PILLAR")) {
        fincept::SymbolContext::instance().set_group_symbol(link_group_, fincept::SymbolRef::equity(sym), this);
    }
}

void SurfaceAnalyticsScreen::on_fetch_requested() {
    if (!control_panel_)
        return;
    const auto& cap = capability_for(active_chart_);
    if (cap.tier == SurfaceTier::DEMO)
        return; // Button should be disabled, but guard anyway

    auto& svc = DatabentoService::instance();
    if (!svc.has_api_key()) {
        if (data_inspector_) {
            data_inspector_->set_status(tr("No Databento API key configured"), false);
            data_inspector_->set_error(tr("Add a key in Settings → Credentials → Databento."));
        }
        return;
    }

    // Option-surface fetches take the spot as a hard input (strike window around it, IV
    // solved against it). spot_for() falls back to a 100.0 placeholder when no quote is
    // known - fine for sample data, but sending it makes e.g. a $600 underlying return
    // an empty or nonsensical grid that is then labelled as live data. Wait for a quote.
    const bool spot_dependent =
        active_chart_ == ChartType::Volatility || active_chart_ == ChartType::DeltaSurface ||
        active_chart_ == ChartType::GammaSurface || active_chart_ == ChartType::VegaSurface ||
        active_chart_ == ChartType::ThetaSurface || active_chart_ == ChartType::SkewSurface ||
        active_chart_ == ChartType::LocalVolSurface || active_chart_ == ChartType::ImpliedDividend ||
        active_chart_ == ChartType::LiquidityHeatmap;
    if (spot_dependent && !has_live_spot(control_panel_->state().symbol)) {
        const QString sym = control_panel_->state().symbol;
        fincept::datahub::DataHub::instance().request(QString("market:quote:%1").arg(sym), /*force*/ true);
        if (data_inspector_) {
            data_inspector_->set_status(tr("Waiting for a live %1 quote").arg(sym), false);
            data_inspector_->set_error(
                tr("No live spot price for %1 yet, and an option surface can't be built without one. "
                   "A quote has been requested - press FETCH again in a moment.")
                    .arg(sym));
        }
        return;
    }

    DatabentoFetchParams p;
    static const char* CT_NAMES[] = {
        "Volatility",
        "DeltaSurface",
        "GammaSurface",
        "VegaSurface",
        "ThetaSurface",
        "SkewSurface",
        "LocalVolSurface",
        "YieldCurve",
        "SwaptionVol",
        "CapFloorVol",
        "BondSpread",
        "OISBasis",
        "RealYield",
        "ForwardRate",
        "FXVol",
        "FXForwardPoints",
        "CrossCurrencyBasis",
        "CDSSpread",
        "CreditTransition",
        "RecoveryRate",
        "CommodityForward",
        "CommodityVol",
        "CrackSpread",
        "ContangoBackwardation",
        "Correlation",
        "PCA",
        "VaR",
        "StressTestPnL",
        "FactorExposure",
        "LiquidityHeatmap",
        "Drawdown",
        "BetaSurface",
        "ImpliedDividend",
        "InflationExpectations",
        "MonetaryPolicyPath",
    };
    int idx = (int)active_chart_;
    if (idx >= 0 && idx < (int)(sizeof(CT_NAMES) / sizeof(CT_NAMES[0])))
        p.chart_type = QString::fromUtf8(CT_NAMES[idx]);

    const auto& s = control_panel_->state();
    p.symbol = s.symbol;
    p.basket = s.basket;
    p.dataset = s.dataset.isEmpty() ? QString::fromUtf8(cap.dataset) : s.dataset;
    p.start_date = s.start_date;
    p.end_date = s.end_date;
    p.strike_window_pct = s.strike_window_pct;
    p.dte_min = s.dte_min;
    p.dte_max = s.dte_max;
    p.iv_method = s.iv_method;
    p.spot_override = spot_for(p.symbol);

    if (data_inspector_)
        data_inspector_->set_status(tr("Fetching %1 …").arg(QString::fromUtf8(chart_type_name(active_chart_))), true);
    svc.fetch_with_params(p);
}

void SurfaceAnalyticsScreen::dispatch_csv(const QString& path) {
    // Every failure path here used to `return` silently, so a malformed file, an
    // unreadable path, or an unsupported surface all looked identical to a
    // successful import that changed nothing.
    auto report = [this](const QString& message, bool ok) {
        if (!data_inspector_)
            return;
        data_inspector_->set_status(message, ok);
        if (!ok)
            data_inspector_->set_error(message);
    };

    std::string err;
    auto rows = parse_csv_file(path, err);
    if (rows.empty()) {
        const QString detail = err.empty() ? tr("no data rows found") : QString::fromStdString(err);
        report(tr("CSV import failed: %1").arg(detail), false);
        return;
    }

    // Load into a copy and commit only on success: several loaders reset their target
    // (`out = {}`) before they can fail, which would otherwise blank the surface on a
    // rejected file.
    auto load_into = [&rows, &err](auto& target, auto&& loader) {
        auto staged = target;
        if (!loader(rows, staged, err))
            return false;
        target = std::move(staged);
        return true;
    };
    auto greeks = [](const char* name) {
        return [name](const auto& r, GreeksSurfaceData& o, std::string& e) {
            return load_greeks_surface(r, o, e, name);
        };
    };

    // Every surface has a loader in SurfaceCsvImporter (the header promises "all 35"),
    // but only five were ever reachable from here; the rest answered "not implemented".
    bool loaded = false;
    switch (active_chart_) {
        case ChartType::Volatility:
            loaded = load_into(vol_data_, load_vol_surface);
            break;
        case ChartType::DeltaSurface:
            loaded = load_into(delta_data_, greeks("Delta"));
            break;
        case ChartType::GammaSurface:
            loaded = load_into(gamma_data_, greeks("Gamma"));
            break;
        case ChartType::VegaSurface:
            loaded = load_into(vega_data_, greeks("Vega"));
            break;
        case ChartType::ThetaSurface:
            loaded = load_into(theta_data_, greeks("Theta"));
            break;
        case ChartType::SkewSurface:
            loaded = load_into(skew_data_, load_skew_surface);
            break;
        case ChartType::LocalVolSurface:
            loaded = load_into(local_vol_data_, load_local_vol);
            break;
        case ChartType::YieldCurve:
            loaded = load_into(yield_data_, load_yield_curve);
            break;
        case ChartType::SwaptionVol:
            loaded = load_into(swaption_data_, load_swaption_vol);
            break;
        case ChartType::CapFloorVol:
            loaded = load_into(capfloor_data_, load_capfloor_vol);
            break;
        case ChartType::BondSpread:
            loaded = load_into(bond_spread_data_, load_bond_spread);
            break;
        case ChartType::OISBasis:
            loaded = load_into(ois_data_, load_ois_basis);
            break;
        case ChartType::RealYield:
            loaded = load_into(real_yield_data_, load_real_yield);
            break;
        case ChartType::ForwardRate:
            loaded = load_into(fwd_rate_data_, load_forward_rate);
            break;
        case ChartType::FXVol:
            loaded = load_into(fx_vol_data_, load_fx_vol);
            break;
        case ChartType::FXForwardPoints:
            loaded = load_into(fx_fwd_data_, load_fx_forward);
            break;
        case ChartType::CrossCurrencyBasis:
            loaded = load_into(xccy_data_, load_xccy_basis);
            break;
        case ChartType::CDSSpread:
            loaded = load_into(cds_data_, load_cds_spread);
            break;
        case ChartType::CreditTransition:
            loaded = load_into(credit_trans_data_, load_credit_trans);
            break;
        case ChartType::RecoveryRate:
            loaded = load_into(recovery_data_, load_recovery_rate);
            break;
        case ChartType::CommodityForward:
            loaded = load_into(cmdty_fwd_data_, load_cmdty_forward);
            break;
        case ChartType::CommodityVol:
            loaded = load_into(cmdty_vol_data_, load_cmdty_vol);
            break;
        case ChartType::CrackSpread:
            loaded = load_into(crack_data_, load_crack_spread);
            break;
        case ChartType::ContangoBackwardation:
            loaded = load_into(contango_data_, load_contango);
            break;
        case ChartType::Correlation:
            loaded = load_into(corr_data_, load_correlation);
            break;
        case ChartType::PCA:
            loaded = load_into(pca_data_, load_pca);
            break;
        case ChartType::VaR:
            loaded = load_into(var_data_, load_var);
            break;
        case ChartType::StressTestPnL:
            loaded = load_into(stress_data_, load_stress_test);
            break;
        case ChartType::FactorExposure:
            loaded = load_into(factor_data_, load_factor_exposure);
            break;
        case ChartType::LiquidityHeatmap:
            loaded = load_into(liquidity_data_, load_liquidity);
            break;
        case ChartType::Drawdown:
            loaded = load_into(drawdown_data_, load_drawdown);
            break;
        case ChartType::BetaSurface:
            loaded = load_into(beta_data_, load_beta);
            break;
        case ChartType::ImpliedDividend:
            loaded = load_into(impl_div_data_, load_implied_div);
            break;
        case ChartType::InflationExpectations:
            loaded = load_into(inflation_data_, load_inflation);
            break;
        case ChartType::MonetaryPolicyPath:
            loaded = load_into(monetary_data_, load_monetary);
            break;
    }

    if (!loaded) {
        report(tr("CSV import failed: %1").arg(QString::fromStdString(err)), false);
        return;
    }

    // Imported data is real data — the surface must stop claiming DEMO.
    mark_chart_real(active_chart_);
    update_chart();
    update_metrics();
    update_inspector_lineage();
    report(tr("Imported %1 rows from %2").arg(qint64(rows.size())).arg(QFileInfo(path).fileName()), true);
}

// ── Databento slots ───────────────────────────────────────────────────────────
} // namespace fincept::surface
