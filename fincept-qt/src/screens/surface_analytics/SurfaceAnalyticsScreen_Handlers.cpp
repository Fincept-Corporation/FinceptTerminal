// src/screens/surface_analytics/SurfaceAnalyticsScreen_Handlers.cpp
//
// User-interaction handlers: category/surface selection, view-mode toggles,
// CSV import, refresh, and the symbol/fetch flow that drives Databento
// requests via the control panel.
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

    if (control_panel_)
        control_panel_->set_capability(active_chart_);
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
    if (control_panel_)
        control_panel_->set_capability(active_chart_);
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
    QString path = QFileDialog::getOpenFileName(this, "Import Surface CSV", {}, "CSV Files (*.csv)");
    if (!path.isEmpty())
        dispatch_csv(path);
}

void SurfaceAnalyticsScreen::on_refresh() {
    load_demo_data();
    update_chart();
    update_metrics();
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::on_controls_changed() {
    update_inspector_lineage();
}

void SurfaceAnalyticsScreen::on_control_symbol_changed(const QString& /*sym*/) {
    // Rebuild demo surfaces with the new underlying so the chart isn't stale.
    load_demo_data();
    update_chart();
    update_metrics();
    update_inspector_lineage();
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
            data_inspector_->set_status("No Databento API key configured", false);
            data_inspector_->set_error("Add a key in Settings → Credentials → Databento.");
        }
        return;
    }

    DatabentoFetchParams p;
    static const char* CT_NAMES[] = {
        "Volatility", "DeltaSurface", "GammaSurface", "VegaSurface", "ThetaSurface",
        "SkewSurface", "LocalVolSurface", "YieldCurve", "SwaptionVol", "CapFloorVol",
        "BondSpread", "OISBasis", "RealYield", "ForwardRate", "FXVol",
        "FXForwardPoints", "CrossCurrencyBasis", "CDSSpread", "CreditTransition",
        "RecoveryRate", "CommodityForward", "CommodityVol", "CrackSpread",
        "ContangoBackwardation", "Correlation", "PCA", "VaR", "StressTestPnL",
        "FactorExposure", "LiquidityHeatmap", "Drawdown", "BetaSurface",
        "ImpliedDividend", "InflationExpectations", "MonetaryPolicyPath",
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
        data_inspector_->set_status(
            QString("Fetching %1 …").arg(QString::fromUtf8(chart_type_name(active_chart_))), true);
    svc.fetch_with_params(p);
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
} // namespace fincept::surface
