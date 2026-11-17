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
#include "screens/common/CommandRows.h"
#include "services/markets/MarketDataService.h"
#include "services/options/FreeOptionChainService.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
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
#include <iterator>

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
    // An index root ("SPX") is quoted under its Yahoo ticker ("^SPX").
    auto& hub = fincept::datahub::DataHub::instance();
    QVariant v = hub.peek(QString("market:quote:%1").arg(surface_quote_symbol(sym)));
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
    // spot_symbol_ is the quoted symbol (an index root "SPX" is quoted as "^SPX");
    // spot_cache_ stays keyed by the panel's symbol.
    const QString quoted = surface_quote_symbol(sym);
    const QString topic = QString("market:quote:%1").arg(quoted);
    spot_symbol_ = quoted;
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
    update_source_banner();
    schedule_free_chain(); // an option surface still on sample data loads the delayed chain
}

void SurfaceAnalyticsScreen::refresh_provider_status() {
    if (!control_panel_)
        return;
    auto& svc = DatabentoService::instance();
    control_panel_->set_provider_status("databento", svc.has_api_key() ? "configured" : "not configured",
                                        svc.has_api_key() ? "key set" : "Settings → Credentials");
}

void SurfaceAnalyticsScreen::load_dataset_list() {
    if (!control_panel_)
        return;
    auto& svc = DatabentoService::instance();
    if (!svc.has_api_key())
        return; // the panel keeps its seed list
    QPointer<SurfaceAnalyticsScreen> self = this;
    svc.list_datasets([self](const QStringList& datasets) {
        if (!self || !self->control_panel_)
            return;
        self->control_panel_->set_datasets(datasets);
    });
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
    connect(&svc, &DatabentoService::raw_response, this, &SurfaceAnalyticsScreen::on_db_raw_response,
            Qt::UniqueConnection);
    refresh_provider_status();
    load_dataset_list();
    load_dataset_range_for_active_capability();
    resubscribe_spot();
    update_source_banner();
    schedule_free_chain();
}

void SurfaceAnalyticsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    auto& svc = DatabentoService::instance();
    disconnect(&svc, nullptr, this, nullptr);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    spot_symbol_.clear();
    // Leaving the screen cancels the automatic delayed-chain load (the next show restarts it).
    if (free_req_ != 0) {
        fincept::services::options::FreeOptionChainService::instance().cancel(free_req_);
        free_req_ = 0;
    }
    free_loading_symbol_.clear();
    release_free_gate();
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
    update_source_banner();
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
    // The surface itself ("chart" was saved but never restored: every visit reset to the
    // category's first surface).
    if (state.contains("chart")) {
        const auto cats = get_surface_categories();
        const auto want = static_cast<ChartType>(state.value("chart").toInt());
        if (active_category_ >= 0 && active_category_ < (int)cats.size()) {
            const auto& types = cats[active_category_].types;
            for (int i = 0; i < (int)types.size(); ++i)
                if (types[i] == want && want != active_chart_) {
                    on_surface_clicked(active_category_, i);
                    break;
                }
        }
    }
    if (state.contains("view_mode")) {
        view_mode_ = static_cast<ViewMode>(state.value("view_mode", 0).toInt());
        apply_view_mode_buttons();
    }
    if (control_panel_) {
        SurfaceControlsState cs = control_panel_->state();
        // The remembered underlying; a caret index from older sessions ("^SPX") is the
        // plain root the option sources list ("SPX").
        cs.symbol = surface_us_chain_symbol(state.value("symbol", cs.symbol).toString());
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
    // A linked Yahoo index ("^GSPC") is the option root the surfaces take ("SPX").
    const QString linked = surface_us_chain_symbol(ref.symbol);
    if (linked.isEmpty() || cs.symbol.compare(linked, Qt::CaseInsensitive) == 0)
        return;
    cs.symbol = linked;
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
    if (surface_is_us_index_root(s)) {
        // An index root ("SPX") is the Yahoo index the other screens read ("^SPX").
        fincept::SymbolRef ref;
        ref.symbol = surface_link_symbol(s);
        ref.asset_class = QStringLiteral("index");
        ref.exchange = QStringLiteral("US");
        return ref;
    }
    return fincept::SymbolRef::equity(s);
}

// ── ISectionLinked ───────────────────────────────────────────────────────────

namespace {

// Link ids of get_surface_categories(), in its order.
const char* const kSurfLinkCategories[] = {"equity_deriv", "fixed_income", "fx",  "credit",
                                           "commodities",  "risk",         "macro"};

struct SurfLinkSurface {
    const char* id;
    ChartType type;
};

// Link ids of the 35 surfaces ("surface:<id>").
const SurfLinkSurface kSurfLinkSurfaces[] = {
    {"vol", ChartType::Volatility},
    {"delta", ChartType::DeltaSurface},
    {"gamma", ChartType::GammaSurface},
    {"vega", ChartType::VegaSurface},
    {"theta", ChartType::ThetaSurface},
    {"skew", ChartType::SkewSurface},
    {"local_vol", ChartType::LocalVolSurface},
    {"yield_curve", ChartType::YieldCurve},
    {"swaption_vol", ChartType::SwaptionVol},
    {"cap_floor", ChartType::CapFloorVol},
    {"bond_spread", ChartType::BondSpread},
    {"ois_basis", ChartType::OISBasis},
    {"real_yield", ChartType::RealYield},
    {"forward_rate", ChartType::ForwardRate},
    {"fx_vol", ChartType::FXVol},
    {"fx_forward", ChartType::FXForwardPoints},
    {"xccy_basis", ChartType::CrossCurrencyBasis},
    {"cds_spread", ChartType::CDSSpread},
    {"transition", ChartType::CreditTransition},
    {"recovery", ChartType::RecoveryRate},
    {"commodity_forward", ChartType::CommodityForward},
    {"commodity_vol", ChartType::CommodityVol},
    {"crack_spread", ChartType::CrackSpread},
    {"contango", ChartType::ContangoBackwardation},
    {"correlation", ChartType::Correlation},
    {"pca", ChartType::PCA},
    {"var", ChartType::VaR},
    {"stress_test", ChartType::StressTestPnL},
    {"factor_exposure", ChartType::FactorExposure},
    {"liquidity", ChartType::LiquidityHeatmap},
    {"drawdown", ChartType::Drawdown},
    {"beta", ChartType::BetaSurface},
    {"implied_dividend", ChartType::ImpliedDividend},
    {"inflation", ChartType::InflationExpectations},
    {"policy_path", ChartType::MonetaryPolicyPath},
};

} // namespace

bool SurfaceAnalyticsScreen::open_section(const QString& section, const QString& focus) {
    namespace rows = fincept::screens::command_rows;
    QString key = section.trimmed();
    if (!key.isEmpty()) {
        const auto cats = get_surface_categories();
        int cat = -1;
        int surf = -1;
        QString rest;
        QString surface_id;
        if (rows::split_kind(key, "surface", &rest)) {
            surface_id = rest;
        } else {
            if (rows::split_kind(key, "category", &rest))
                key = rest;
            const qsizetype slash = key.indexOf(QLatin1Char('/'));
            if (slash >= 0) {
                surface_id = key.mid(slash + 1).trimmed();
                key = key.left(slash).trimmed();
            }
            for (int i = 0; i < (int)std::size(kSurfLinkCategories) && i < (int)cats.size(); ++i)
                if (key.compare(QLatin1String(kSurfLinkCategories[i]), Qt::CaseInsensitive) == 0)
                    cat = i;
            if (cat < 0)
                return false;
        }
        if (!surface_id.isEmpty()) {
            const SurfLinkSurface* found = nullptr;
            for (const SurfLinkSurface& s : kSurfLinkSurfaces)
                if (surface_id.compare(QLatin1String(s.id), Qt::CaseInsensitive) == 0)
                    found = &s;
            if (!found)
                return false;
            int in_cat = -1;
            int in_index = -1;
            for (int c = 0; c < (int)cats.size() && in_cat < 0; ++c)
                for (int i = 0; i < (int)cats[c].types.size(); ++i)
                    if (cats[c].types[i] == found->type) {
                        in_cat = c;
                        in_index = i;
                        break;
                    }
            if (in_cat < 0 || (cat >= 0 && cat != in_cat))
                return false; // "<category>/<surface>" naming a surface of another category
            cat = in_cat;
            surf = in_index;
        }
        // The category / surface buttons' click paths (as restore_state() drives them).
        if (cat != active_category_)
            on_category_clicked(cat);
        if (surf >= 0 && cats[cat].types[surf] != active_chart_)
            on_surface_clicked(cat, surf);
    }
    const QString symbol = focus.trimmed();
    if (!symbol.isEmpty())
        on_group_symbol_changed(fincept::SymbolRef::equity(symbol)); // the linked-symbol path
    return true;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

struct SurfLinkRow {
    const char* section;
    const char* title;
    const char* keywords;
};

const SurfLinkRow kSurfLinkRows[] = {
    // Categories (the seed's "surface_analytics.<category>" rows are replaced)
    {"equity_deriv", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Equity derivative surfaces"),
     "options surfaces, vol surface, greeks, skew, implied volatility"},
    {"fixed_income", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Fixed income surfaces"),
     "rates, yield curve, swaptions, bonds, interest rates"},
    {"fx", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "FX surfaces"),
     "currency volatility, fx options, forward points, cross currency basis"},
    {"credit", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Credit surfaces"),
     "credit spreads, cds, default, rating transitions"},
    {"commodities", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Commodity surfaces"),
     "futures curve, commodity volatility, oil, crack spread"},
    {"risk", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Risk surfaces"),
     "correlation, pca, var, stress test, drawdown, beta, factor exposure"},
    {"macro", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Macro surfaces"),
     "inflation expectations, monetary policy, rate path"},
    // Surfaces a beginner looks for by name
    {"surface:vol", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Volatility surface"),
     "vol surface, implied volatility, iv surface, 3d volatility"},
    {"surface:skew", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Volatility skew"),
     "skew, smile, volatility smile, put skew"},
    {"surface:local_vol", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Local volatility surface"),
     "local vol, dupire"},
    {"surface:yield_curve", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Yield curve surface"),
     "yield curve, term structure, treasury curve, rates over time"},
    {"surface:swaption_vol", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Swaption volatility cube"),
     "swaption, swaption vol, rates volatility"},
    {"surface:fx_vol", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "FX volatility surface"),
     "fx vol, currency options, eurusd volatility"},
    {"surface:cds_spread", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "CDS spread surface"),
     "cds, credit default swap, credit spread curve"},
    {"surface:commodity_forward", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Commodity futures curve"),
     "futures curve, forward curve, oil futures, term structure"},
    {"surface:contango", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Contango & backwardation"),
     "contango, backwardation, roll yield"},
    {"surface:correlation", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Correlation matrix"),
     "correlation, correlation heatmap, co-movement"},
    {"surface:pca", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "PCA factor surface"),
     "pca, principal components, factors"},
    {"surface:var", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Value at risk surface"),
     "var, value at risk, tail risk"},
    {"surface:stress_test", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Stress test P&L"),
     "stress test, scenario, shocks, pnl"},
    {"surface:inflation", QT_TRANSLATE_NOOP("fincept::surface::SurfaceAnalyticsScreen", "Inflation expectations"),
     "inflation, breakevens, cpi expectations"},
};

QList<command::CommandItem> surf_command_rows() {
    namespace rows = fincept::screens::command_rows;
    const QString where = SurfaceAnalyticsScreen::tr("Surface Analytics");
    QList<command::CommandItem> out;
    for (const SurfLinkRow& r : kSurfLinkRows) {
        QStringList keywords;
        for (const QString& w : QString::fromLatin1(r.keywords).split(QLatin1Char(','), Qt::SkipEmptyParts))
            keywords.append(w.trimmed());
        keywords << QStringLiteral("surface") << QStringLiteral("3d");
        out.append(rows::section(QStringLiteral("surface_analytics"), QString::fromLatin1(r.section),
                                 QCoreApplication::translate("fincept::surface::SurfaceAnalyticsScreen", r.title),
                                 keywords, where));
    }
    return out;
}

void surf_install_commands() {
    fincept::screens::command_rows::install(QStringLiteral("surface_analytics"), &surf_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(surf_install_commands)

} // namespace fincept::surface
