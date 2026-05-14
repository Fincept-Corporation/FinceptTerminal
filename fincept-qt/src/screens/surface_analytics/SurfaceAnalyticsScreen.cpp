// src/screens/surface_analytics/SurfaceAnalyticsScreen.cpp
//
// Core lifecycle: symbol/spot helpers, show/hide events, IStatefulScreen +
// IGroupLinked overrides, provider status refresh, and dataset-range loading.
// The rest of the implementation lives in:
//   - SurfaceAnalyticsScreen_Layout.cpp    — setup_ui, build_* bars + helpers
//   - SurfaceAnalyticsScreen_Views.cpp     — update_chart / metrics / line / demo
//   - SurfaceAnalyticsScreen_Handlers.cpp  — click / view-mode / fetch handlers
//   - SurfaceAnalyticsScreen_Providers.cpp — Databento result handlers
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

#include <cstdlib>
#include <ctime>

namespace fincept::surface {

using namespace fincept::ui;

QString SurfaceAnalyticsScreen::current_symbol_or_default() const {
    QString s = control_panel_ ? control_panel_->state().symbol : QString();
    if (s.isEmpty())
        s = QString::fromUtf8(defaults::EQUITY_UNDERLYINGS[0]);
    return s;
}

float SurfaceAnalyticsScreen::spot_for(const QString& sym) const {
    if (sym.isEmpty())
        return 100.0f;
    auto it = spot_cache_.constFind(sym);
    if (it != spot_cache_.constEnd())
        return it.value();
    // Best-effort hub lookup; if no quote published yet, returns invalid QVariant.
    auto& hub = fincept::datahub::DataHub::instance();
    QVariant v = hub.peek(QString("market:quote:%1").arg(sym));
    if (v.isValid()) {
        if (v.canConvert<fincept::services::QuoteData>()) {
            auto q = v.value<fincept::services::QuoteData>();
            if (q.price > 0.0)
                return (float)q.price;
        }
        bool ok = false;
        double d = v.toDouble(&ok);
        if (ok && d > 0.0)
            return (float)d;
    }
    return 100.0f;
}


void SurfaceAnalyticsScreen::refresh_provider_status() {
    if (!control_panel_)
        return;
    auto& svc = DatabentoService::instance();
    control_panel_->set_provider_status(
        "databento",
        svc.has_api_key() ? "configured" : "not configured",
        svc.has_api_key() ? "key set" : "Settings → Credentials");
}

void SurfaceAnalyticsScreen::load_dataset_range_for_active_capability() {
    if (!control_panel_)
        return;
    const auto& cap = capability_for(active_chart_);
    QString ds = QString::fromUtf8(cap.dataset);
    if (ds.isEmpty())
        return; // DEMO surface — no Databento dataset to query
    auto& svc = DatabentoService::instance();
    if (!svc.has_api_key())
        return;
    QPointer<SurfaceAnalyticsScreen> self = this;
    svc.get_dataset_range(ds, [self](DbDatasetRange r) {
        if (!self || !self->control_panel_)
            return;
        self->control_panel_->apply_dataset_range(r.start, r.end);
    });
}

// ── Show/hide event — P3 compliance ──────────────────────────────────────────
void SurfaceAnalyticsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    auto& svc = DatabentoService::instance();
    connect(&svc, &DatabentoService::vol_surface_ready, this,
            &SurfaceAnalyticsScreen::on_vol_surface_received, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::ohlcv_ready, this,
            &SurfaceAnalyticsScreen::on_ohlcv_received, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::futures_ready, this,
            &SurfaceAnalyticsScreen::on_futures_received, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::surface_ready, this,
            &SurfaceAnalyticsScreen::on_surface_received, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::fetch_started, this,
            &SurfaceAnalyticsScreen::on_db_fetch_started, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::fetch_failed, this,
            &SurfaceAnalyticsScreen::on_db_fetch_failed, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::connection_tested, this,
            &SurfaceAnalyticsScreen::on_db_connection_tested, Qt::UniqueConnection);
    connect(&svc, &DatabentoService::raw_response, this,
            &SurfaceAnalyticsScreen::on_db_raw_response, Qt::UniqueConnection);
    refresh_provider_status();
    load_dataset_range_for_active_capability();
}

void SurfaceAnalyticsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    auto& svc = DatabentoService::instance();
    disconnect(&svc, nullptr, this, nullptr);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap SurfaceAnalyticsScreen::save_state() const {
    QVariantMap s{
        {"category", active_category_},
        {"chart", static_cast<int>(active_chart_)},
        {"view_mode", static_cast<int>(view_mode_)},
    };
    if (control_panel_) {
        const auto& cs = control_panel_->state();
        s["symbol"] = cs.symbol;
        s["dataset"] = cs.dataset;
        s["start_date"] = cs.start_date.toString(Qt::ISODate);
        s["end_date"] = cs.end_date.toString(Qt::ISODate);
        s["strike_window_pct"] = cs.strike_window_pct;
        s["dte_min"] = cs.dte_min;
        s["dte_max"] = cs.dte_max;
        s["iv_method"] = cs.iv_method;
        s["basket"] = cs.basket;
    }
    return s;
}

void SurfaceAnalyticsScreen::restore_state(const QVariantMap& state) {
    const int cat = state.value("category", 0).toInt();
    if (cat != active_category_)
        on_category_clicked(cat);
    if (state.contains("view_mode")) {
        view_mode_ = static_cast<ViewMode>(state.value("view_mode", 0).toInt());
        apply_view_mode_buttons();
    }
    if (control_panel_) {
        SurfaceControlsState cs = control_panel_->state();
        cs.symbol = state.value("symbol", cs.symbol).toString();
        cs.dataset = state.value("dataset", cs.dataset).toString();
        QString sd = state.value("start_date").toString();
        QString ed = state.value("end_date").toString();
        if (!sd.isEmpty()) cs.start_date = QDate::fromString(sd, Qt::ISODate);
        if (!ed.isEmpty()) cs.end_date = QDate::fromString(ed, Qt::ISODate);
        cs.strike_window_pct = state.value("strike_window_pct", cs.strike_window_pct).toInt();
        cs.dte_min = state.value("dte_min", cs.dte_min).toInt();
        cs.dte_max = state.value("dte_max", cs.dte_max).toInt();
        cs.iv_method = state.value("iv_method", cs.iv_method).toString();
        cs.basket = state.value("basket", cs.basket).toStringList();
        control_panel_->apply_state(cs);
        load_demo_data();
        update_chart();
        update_metrics();
    }
}

// ── IGroupLinked ─────────────────────────────────────────────────────────────

void SurfaceAnalyticsScreen::on_group_symbol_changed(const fincept::SymbolRef& ref) {
    if (!ref.is_valid() || !control_panel_)
        return;
    // Push the linked symbol into the control panel; demo data + chart rebuild
    // happen via on_control_symbol_changed.
    SurfaceControlsState cs = control_panel_->state();
    if (cs.symbol.compare(ref.symbol, Qt::CaseInsensitive) == 0)
        return;
    cs.symbol = ref.symbol.toUpper();
    control_panel_->apply_state(cs);
    on_control_symbol_changed(cs.symbol);
}

fincept::SymbolRef SurfaceAnalyticsScreen::current_symbol() const {
    if (!control_panel_)
        return {};
    QString s = control_panel_->state().symbol;
    if (s.isEmpty())
        return {};
    return fincept::SymbolRef::equity(s);
}

} // namespace fincept::surface
