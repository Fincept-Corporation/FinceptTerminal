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
#include <QVBoxLayout>
#include <QVariant>

#include <QPointer>

#include <cmath>
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

namespace {
// Live spot for `sym` from the subscription cache or a hub snapshot. 0 = none known.
float surface_live_spot(const QHash<QString, float>& cache, const QString& sym) {
    if (sym.isEmpty())
        return 0.0f;
    auto it = cache.constFind(sym);
    if (it != cache.constEnd() && it.value() > 0.0f)
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
    return 0.0f;
}
} // namespace

float SurfaceAnalyticsScreen::spot_for(const QString& sym) const {
    // 100.0 is a PLACEHOLDER for sample-data generation only; has_live_spot() tells
    // callers whether it is safe to send a spot to a data provider.
    const float live = surface_live_spot(spot_cache_, sym);
    return live > 0.0f ? live : 100.0f;
}

bool SurfaceAnalyticsScreen::has_live_spot(const QString& sym) const {
    return surface_live_spot(spot_cache_, sym) > 0.0f;
}

void SurfaceAnalyticsScreen::resubscribe_spot() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (!spot_symbol_.isEmpty()) {
        hub.unsubscribe(this, QString("market:quote:%1").arg(spot_symbol_));
        spot_symbol_.clear();
    }
    if (!isVisible())
        return; // showEvent re-arms it (P3 / D3: subscribe only while visible)
    const QString sym = current_symbol_or_default();
    if (sym.isEmpty())
        return;
    const QString topic = QString("market:quote:%1").arg(sym);
    spot_symbol_ = sym;
    QPointer<SurfaceAnalyticsScreen> self = this;
    hub.subscribe(this, topic, [self, sym](const QVariant& v) {
        if (!self)
            return;
        double price = 0.0;
        if (v.canConvert<fincept::services::QuoteData>())
            price = v.value<fincept::services::QuoteData>().price;
        else
            price = v.toDouble();
        if (!(price > 0.0))
            return;
        const float prev = self->spot_cache_.value(sym, 0.0f);
        self->spot_cache_.insert(sym, float(price));
        if (self->current_symbol_or_default() != sym)
            return;
        if (self->control_panel_)
            self->control_panel_->set_spot(price);
        // The sample surfaces on screen were generated off the 100.0 placeholder (or an
        // older spot); rebuild them around the real level. Never when any grid holds
        // fetched/imported data - load_demo_data() would discard it.
        if (self->real_data_charts_.isEmpty() && (prev <= 0.0f || std::abs(price - prev) / prev > 0.01)) {
            self->load_demo_data();
            self->update_chart();
            self->update_metrics();
            self->update_inspector_lineage();
        }
    });
    hub.request(topic, /*force*/ false);
}

void SurfaceAnalyticsScreen::mark_chart_real(ChartType type) {
    real_data_charts_.insert(static_cast<int>(type));
    sync_synthetic_badge();
}

void SurfaceAnalyticsScreen::sync_synthetic_badge() {
    const bool synthetic = !real_data_charts_.contains(static_cast<int>(active_chart_));
    if (control_panel_)
        control_panel_->mark_synthetic(synthetic);
    if (demo_banner_)
        demo_banner_->setVisible(synthetic);
}

void SurfaceAnalyticsScreen::refresh_provider_status() {
    if (!control_panel_)
        return;
    auto& svc = DatabentoService::instance();
    control_panel_->set_provider_status("databento", svc.has_api_key() ? "configured" : "not configured",
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
    connect(&svc, &DatabentoService::vol_surface_ready, this, &SurfaceAnalyticsScreen::on_vol_surface_received,
            Qt::UniqueConnection);
    connect(&svc, &DatabentoService::ohlcv_ready, this, &SurfaceAnalyticsScreen::on_ohlcv_received,
            Qt::UniqueConnection);
    connect(&svc, &DatabentoService::futures_ready, this, &SurfaceAnalyticsScreen::on_futures_received,
            Qt::UniqueConnection);
    connect(&svc, &DatabentoService::surface_ready, this, &SurfaceAnalyticsScreen::on_surface_received,
            Qt::UniqueConnection);
    connect(&svc, &DatabentoService::fetch_started, this, &SurfaceAnalyticsScreen::on_db_fetch_started,
            Qt::UniqueConnection);
    connect(&svc, &DatabentoService::fetch_failed, this, &SurfaceAnalyticsScreen::on_db_fetch_failed,
            Qt::UniqueConnection);
    connect(&svc, &DatabentoService::connection_tested, this, &SurfaceAnalyticsScreen::on_db_connection_tested,
            Qt::UniqueConnection);
    connect(&svc, &DatabentoService::raw_response, this, &SurfaceAnalyticsScreen::on_db_raw_response,
            Qt::UniqueConnection);
    refresh_provider_status();
    load_dataset_range_for_active_capability();
    resubscribe_spot();
}

void SurfaceAnalyticsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    auto& svc = DatabentoService::instance();
    disconnect(&svc, nullptr, this, nullptr);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    spot_symbol_.clear();
}

// ── Live language switch ──────────────────────────────────────────────────────
void SurfaceAnalyticsScreen::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(e);
}

void SurfaceAnalyticsScreen::retranslateUi() {
    // Fixed-label toolbar buttons. Category/surface chips carry data-domain
    // names (chart_type_name / category name) and are not translated.
    if (import_btn_)
        import_btn_->setText(tr("IMPORT CSV"));
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));
    if (btn_3d_)
        btn_3d_->setText(tr("3D"));
    if (btn_table_)
        btn_table_->setText(tr("TABLE"));
    if (btn_line_)
        btn_line_->setText(tr("LINE"));
    if (demo_banner_)
        demo_banner_->setText(tr("SAMPLE DATA — synthetic values, not a live market surface. "
                                 "Do not trade or quote from this."));
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
        if (!sd.isEmpty())
            cs.start_date = QDate::fromString(sd, Qt::ISODate);
        if (!ed.isEmpty())
            cs.end_date = QDate::fromString(ed, Qt::ISODate);
        cs.strike_window_pct = state.value("strike_window_pct", cs.strike_window_pct).toInt();
        cs.dte_min = state.value("dte_min", cs.dte_min).toInt();
        cs.dte_max = state.value("dte_max", cs.dte_max).toInt();
        cs.iv_method = state.value("iv_method", cs.iv_method).toString();
        cs.basket = state.value("basket", cs.basket).toStringList();
        control_panel_->apply_state(cs);
        resubscribe_spot();
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
    applying_group_symbol_ = true;
    on_control_symbol_changed(cs.symbol);
    applying_group_symbol_ = false;
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
