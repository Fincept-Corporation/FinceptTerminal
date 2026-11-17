// src/screens/economics/panels/MarketModelsPanel.cpp
// Market Models panel (see MarketModelsPanel.h). Payloads, as the scripts print them:
//   business_cycle.py get_indicators '{"timeout": 20}' — one flat object:
//     sp500, vix, treasury_3m_yield (bond-equivalent), treasury_3m_discount_rate, treasury_10y_yield,
//     treasury_30y_yield, gold_price, crude_oil_price, us_dollar_index — each with <name>_change_pct, the
//     yields also <name>_change_bp; yield_curve_spread_10y_3m / _30y_10y (percentage points),
//     yield_curve_inverted; gold_oil_ratio; sp500_52w_high / _low, sp500_sma_50 / _200, sp500_above_sma200,
//     sp500_ytd_return_pct (+ sp500_ytd_base_date), sp500_1y_return_pct, sp500_stats_window {from, to,
//     observations}; market_sentiment; sources {name: {ticker, provider, unit, as_of}};
//     unavailable [{name, ticker, reason}]; as_of; generated_at. Any series may be null.
//   equity_risk_premium.py estimate_erp '{...}' — decimal fractions per year:
//     erp_estimates {historical_<N>y | implied_earnings_yield | supply_side: {erp | null, components…,
//     method | reason}}, consensus_erp_estimate, consensus_method, erp_range_low / _high,
//     real_risk_free_rate_10y, inputs {name: {value, source, as_of}}, units, notes?, as_of.
//   federal_reserve_data.py yield_curve — {data: [{date, maturity: "month_1" … "year_30", rate (decimal)}]}:
//     the H.15 constant-maturity Treasury yields, i.e. the Treasury par curve, latest date. No API key.
//   fixedIncome/cli.py — yields as DECIMALS (percent input is rejected):
//     curve_shape {maturities, yields} → curve_shape, slope_regime, short/mid/long rate + maturity,
//         slope_bps, butterfly_bps, spread_2s10s, spread_3m10y (bp), market_signal, notes?
//     bootstrap {maturities, yields, frequency} → spot_curve [{maturity, spot_rate (annual comp.),
//         spot_rate_periodic, spot_rate_continuous, discount_factor, …}]
//     forward_curve {spot_rates: [{maturity, spot_rate}]} → forward_curve [{start, end, forward_rate, …}]
//     nelson_siegel {maturities, yields} (≥ 4 points) → parameters, fitted_curve, fitted_at_inputs,
//         fit_statistics
// Every script reports a failure as {"error": "<plain message>"}; EconomicsService turns that into a
// failed result, which the view shows in the panel's error state.
#include "screens/economics/panels/MarketModelsPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/DataGridEditor.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QPointF>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace fincept::screens {
namespace market_models_panel_detail {

constexpr const char* kSourceId = "market_models";
constexpr const char* kColor = "#14B8A6"; // teal — identity colour, drawn through legible() / tint()
constexpr const char* kCtx = "fincept::screens::MarketModelsPanel";

constexpr const char* kCycleScript = "Analytics/economics/business_cycle.py";
constexpr const char* kErpScript = "Analytics/economics/equity_risk_premium.py";
constexpr const char* kFiScript = "Analytics/fixedIncome/cli.py";
constexpr const char* kUstScript = "federal_reserve_data.py";
constexpr const char* kCycleRequest = "mm_cycle";
constexpr const char* kUstRequest = "mm_ust";
constexpr const char* kFiRunMarker = "mm_fi_run"; // the curve view's "pending" while its analytics run
constexpr int kNetTimeoutSec = 20;                 // per network call inside the economics scripts

constexpr const char* kSrcTreasury = "treasury";
constexpr const char* kSrcManual = "manual";
constexpr const char* kStateKey = "market_models";

namespace fmt = fincept::ui::formatting;

double no_value() {
    return std::numeric_limits<double>::quiet_NaN();
}
bool has(double v) {
    return std::isfinite(v);
}
double num(const QJsonValue& v) {
    return v.isDouble() ? v.toDouble() : no_value();
}
double num(const QJsonObject& o, const char* key) {
    return num(o.value(QLatin1String(key)));
}
QString str(const QJsonObject& o, const char* key) {
    return o.value(QLatin1String(key)).toString().trimmed();
}

QString json_text(const QJsonObject& o) {
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

/// Short stable tag of a request's arguments: a result for older inputs never passes for the current one.
QString args_tag(const QString& args) {
    return QString::fromLatin1(QCryptographicHash::hash(args.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
}

/// A value already in percent ("4.23%").
QString level_pct(double v, int decimals = 2) {
    return has(v) ? fmt::pct(v, decimals, false) : fmt::placeholder();
}
/// A decimal fraction as a percentage ("13.36%").
QString frac_pct(double v, int decimals = 2) {
    return has(v) ? fmt::fraction_pct(v, decimals, false) : fmt::placeholder();
}
QString plain(double v, int decimals = 2) {
    return has(v) ? fmt::number(v, decimals) : fmt::placeholder();
}
QString bps(double v, int decimals = 0) {
    return has(v) ? fmt::bp(v, decimals) : fmt::placeholder();
}

/// "1M", "6M", "2Y", "2.5Y" — a maturity in years as a tenor label (market notation, not translated).
QString tenor_label(double years) {
    if (!has(years) || years <= 0.0)
        return QStringLiteral("0");
    const auto whole = [](double v, double tol) { return std::abs(v - std::round(v)) < tol; };
    if (years < 1.0 - 1e-9) {
        const double months = years * 12.0;
        if (whole(months, 0.02) && std::round(months) >= 1.0)
            return QStringLiteral("%1M").arg(std::llround(months));
        const double weeks = years * 52.0;
        if (whole(weeks, 0.02) && std::round(weeks) >= 1.0)
            return QStringLiteral("%1W").arg(std::llround(weeks));
        return QStringLiteral("%1Y").arg(QString::number(years, 'g', 3));
    }
    if (whole(years, 1e-6))
        return QStringLiteral("%1Y").arg(std::llround(years));
    return QStringLiteral("%1Y").arg(QString::number(years, 'g', 4));
}

/// "3M", "10Y", "2W", "30D", "1.5y" → years; NaN when the text is not a tenor.
double tenor_years(const QString& text) {
    static const QRegularExpression kTenor(QStringLiteral("^\\s*([0-9]+(?:[.,][0-9]+)?)\\s*([DWMY])\\s*$"),
                                           QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = kTenor.match(text);
    if (!m.hasMatch())
        return no_value();
    QString n = m.captured(1);
    n.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = n.toDouble(&ok);
    if (!ok)
        return no_value();
    switch (m.captured(2).toUpper().at(0).toLatin1()) {
        case 'D':
            return v / 365.0;
        case 'W':
            return v / 52.0;
        case 'M':
            return v / 12.0;
        default:
            return v;
    }
}

/// H.15 column names ("month_3", "year_10") → years.
double h15_years(const QString& key) {
    static const QRegularExpression kKey(QStringLiteral("^(week|month|year)_([0-9]+)$"));
    const QRegularExpressionMatch m = kKey.match(key);
    if (!m.hasMatch())
        return no_value();
    const double n = m.captured(2).toDouble();
    if (m.captured(1) == QLatin1String("week"))
        return n / 52.0;
    if (m.captured(1) == QLatin1String("month"))
        return n / 12.0;
    return n;
}

bool is_maturity_text(const QString& s) {
    return has(tenor_years(s)) || ui::DataGridEditor::parse_number(s, nullptr);
}

/// Pasted curve rows → [maturity in years, yield]. Accepts tenor labels (3M, 10Y), "10Y 4.25" in one cell
/// and a leading label column ("UST 10Y | 10 | 4.25").
QVector<QStringList> adapt_curve_paste(const QVector<QStringList>& cells) {
    static const QRegularExpression kSpace(QStringLiteral("\\s+"));
    QVector<QStringList> out;
    out.reserve(cells.size());
    for (QStringList c : cells) {
        if (c.size() == 1)
            c = c.first().trimmed().split(kSpace, Qt::SkipEmptyParts);
        if (c.size() >= 3 && !is_maturity_text(c.at(0)) && is_maturity_text(c.at(1)))
            c.removeFirst();
        QString m = c.value(0).trimmed();
        const double t = tenor_years(m);
        if (has(t))
            m = QString::number(t, 'g', 10);
        out << QStringList{m, c.value(1).trimmed()};
    }
    return out;
}

/// The chart's maturity axis: square-root years spreads out the short end of the curve.
double axis_x(double years) {
    return std::sqrt(std::max(0.0, years));
}

// ── Tiles ─────────────────────────────────────────────────────────────────────
// Labels are translated at use (QCoreApplication::translate with the panel's context).
struct TileDef {
    const char* key;
    const char* label;
};

const TileDef kCycleRates[] = {
    {"t3m", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "3M T-BILL")},
    {"t10", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "10Y TREASURY")},
    {"t30", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "30Y TREASURY")},
    {"s10_3", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "10Y − 3M SPREAD")},
    {"s30_10", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "30Y − 10Y SPREAD")},
};
const TileDef kCycleEquity[] = {
    {"spx", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "S&P 500")},
    {"vix", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "VIX")},
    {"ytd", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "S&P 500 YTD")},
    {"y1", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "S&P 500 1Y")},
    {"range", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "52-WEEK RANGE")},
    {"sma", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "50D / 200D AVERAGE")},
};
const TileDef kCycleCmdty[] = {
    {"gold", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "GOLD")},
    {"oil", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "WTI CRUDE")},
    {"dxy", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "DOLLAR INDEX")},
    {"go", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "GOLD / OIL")},
};
const TileDef kErpTiles[] = {
    {"consensus", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "CONSENSUS ERP")},
    {"range", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "ERP RANGE")},
    {"rf", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "10Y RISK-FREE")},
    {"real", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "REAL 10Y RATE")},
    {"infl", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "EXPECTED INFLATION")},
};
const TileDef kCurveTiles[] = {
    {"shape", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "SHAPE")},
    {"slope", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "SLOPE")},
    {"s2s10s", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "2s10s")},
    {"s3m10y", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "3M10Y")},
    {"fly", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "BUTTERFLY")},
    {"signal", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "SIGNAL")},
};

template <std::size_t N>
ui::KpiStrip* make_strip(const TileDef (&defs)[N], QWidget* parent) {
    auto* strip = new ui::KpiStrip(parent);
    strip->set_spacing(6);
    for (const TileDef& d : defs)
        strip->add_tile(QString::fromLatin1(d.key), QCoreApplication::translate(kCtx, d.label));
    return strip;
}

template <std::size_t N>
void relabel(ui::KpiStrip* strip, const TileDef (&defs)[N]) {
    if (!strip)
        return;
    for (const TileDef& d : defs)
        if (ui::StatTile* t = strip->tile(QString::fromLatin1(d.key)))
            t->set_label(QCoreApplication::translate(kCtx, d.label));
}

template <std::size_t N>
void reset_tiles(ui::KpiStrip* strip, const TileDef (&defs)[N]) {
    if (!strip)
        return;
    for (const TileDef& d : defs) {
        ui::StatTile* t = strip->tile(QString::fromLatin1(d.key));
        if (!t)
            continue;
        t->set_state(ui::StatTile::State::Normal);
        t->set_value(fmt::placeholder());
        t->set_sub_label({});
        t->clear_delta();
        t->set_tone(ui::StatTile::Tone::Neutral);
        t->set_tooltip_text({});
    }
}

/// Switch a label between the page stylesheet's roles (mmNote / mmWarn / mmError …).
void set_role(QLabel* label, const char* role) {
    if (!label || label->objectName() == QLatin1String(role))
        return;
    label->setObjectName(QLatin1String(role));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

/// `wrap` labels are notes / values: wrapped and selectable. Others are captions.
QLabel* make_label(QWidget* parent, const char* role, bool wrap = true) {
    auto* l = new QLabel(parent);
    l->setObjectName(QLatin1String(role));
    l->setWordWrap(wrap);
    if (wrap)
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QTableWidget* make_table(QWidget* parent) {
    auto* t = new QTableWidget(parent);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::ExtendedSelection);
    t->verticalHeader()->setVisible(false);
    t->setAlternatingRowColors(true);
    t->setShowGrid(true);
    t->horizontalHeader()->setHighlightSections(false);
    return t;
}

void set_cell(QTableWidget* t, int row, int col, const QString& text, bool numeric, const QString& tip = {}) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(numeric ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter));
    if (!tip.isEmpty())
        item->setToolTip(tip);
    t->setItem(row, col, item);
}

QJsonValue json_num(double v) {
    return has(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null);
}

/// Row of `arr` whose `key` equals `m` (maturities compare within 1e-4 years).
QJsonObject row_at(const QJsonArray& arr, const char* key, double m) {
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const double x = num(o, key);
        if (has(x) && std::abs(x - m) < 1e-4)
            return o;
    }
    return {};
}

// ── ERP overrides ─────────────────────────────────────────────────────────────
struct ErpField {
    const char* key; ///< estimate_erp parameter
    const char* label;
    const char* placeholder;
    const char* tip;
    double lo; ///< accepted range, in the unit the user types (percent, or a multiple)
    double hi;
    bool percent; ///< typed in percent, sent as a decimal fraction
};

const ErpField kErpFields[] = {
    {"risk_free_rate", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "RISK-FREE 10Y (%)"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "auto: ^TNX"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel",
                       "Nominal 10-year Treasury yield in percent. Blank = the latest ^TNX close (Yahoo Finance)."),
     -5.0, 50.0, true},
    {"expected_inflation", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "EXPECTED INFLATION (%)"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "auto: breakeven or 2"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel",
                       "Expected annual inflation in percent. Blank = the FRED 10-year breakeven (T10YIE) when "
                       "FRED_API_KEY is set, otherwise the Federal Reserve's 2 % goal, labelled as an assumption."),
     -5.0, 50.0, true},
    {"pe_ratio", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "P/E"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "auto: SPY"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel",
                       "S&P 500 price / earnings multiple for the earnings-yield method. Blank = SPY trailing P/E."),
     0.0, 1000.0, false},
    {"dividend_yield", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "DIVIDEND YIELD (%)"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "auto: SPY 12m"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel",
                       "S&P 500 dividend yield in percent. Blank = SPY distributions of the last 12 months over the "
                       "last SPY close."),
     0.0, 50.0, true},
    {"buyback_yield", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "BUYBACK YIELD (%)"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "needed for supply side"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel",
                       "Net buyback yield of the S&P 500 in percent. There is no free source for it, so the "
                       "supply-side ERP is computed only when you enter it together with real earnings growth."),
     -20.0, 50.0, true},
    {"real_earnings_growth", QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "REAL EARNINGS GROWTH (%)"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "needed for supply side"),
     QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel",
                       "Long-run real earnings growth in percent (about real GDP growth). Needed, with the buyback "
                       "yield, for the supply-side ERP."),
     -20.0, 50.0, true},
};
static_assert(sizeof(kErpFields) / sizeof(kErpFields[0]) == 6, "kErpFieldCount must match kErpFields");

/// Methods in reading order: historical windows (shortest first), implied, supply side, anything new.
QStringList ordered_methods(const QJsonObject& estimates) {
    static const QRegularExpression kHist(QStringLiteral("^historical_([0-9]+)y$"));
    QList<QPair<int, QString>> hist;
    QStringList rest;
    for (auto it = estimates.begin(); it != estimates.end(); ++it) {
        const QRegularExpressionMatch m = kHist.match(it.key());
        if (m.hasMatch())
            hist.append(qMakePair(m.captured(1).toInt(), it.key()));
        else if (it.key() != QLatin1String("implied_earnings_yield") && it.key() != QLatin1String("supply_side"))
            rest << it.key();
    }
    std::sort(hist.begin(), hist.end());
    QStringList out;
    for (const auto& h : std::as_const(hist))
        out << h.second;
    for (const char* k : {"implied_earnings_yield", "supply_side"})
        if (estimates.contains(QLatin1String(k)))
            out << QString::fromLatin1(k);
    std::sort(rest.begin(), rest.end());
    return out + rest;
}

QString humanize(QString key) {
    key.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!key.isEmpty())
        key[0] = key[0].toUpper();
    return key;
}

} // namespace market_models_panel_detail

namespace mmd = market_models_panel_detail;

// ── Construction ──────────────────────────────────────────────────────────────

MarketModelsPanel::MarketModelsPanel(QWidget* parent) : EconPanelBase(mmd::kSourceId, mmd::kColor, parent) {
    build_base_ui(this);
    set_stats_visible(false); // tiles, cards and tables — never one dated series for the stat cards
    // Nothing else is built or run here: each view's page is built the first time it is needed, and the
    // results pane runs the visible view once the panel is on screen.
    apply_texts();
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &MarketModelsPanel::on_result);
    sync_load_view();
}

EconPanelBase::LoadPolicy MarketModelsPanel::load_policy() const {
    LoadPolicy p;
    p.shape = ui::LoadingSkeleton::Shape::Cards;
    p.max_age_s = 30 * 60; // market data: refreshed quietly when the remembered result is older
    p.timeout_s = 150;
    return p;
}

QString MarketModelsPanel::view_key(int view) {
    switch (view) {
        case kErp:
            return QStringLiteral("erp");
        case kCurve:
            return QStringLiteral("curve");
        default:
            return QStringLiteral("cycle");
    }
}

int MarketModelsPanel::view_of_page(int page) const {
    for (int v = 0; v < kViewCount; ++v)
        if (page >= 0 && tabs_[v].page == page)
            return v;
    return -1;
}

void MarketModelsPanel::sync_load_view() {
    const int v = current_view();
    // The views share the result widgets: a run for the view being left is stopped (it would draw
    // over the view now on screen); it runs again when its view is shown.
    if (query_running() && load_pane() && load_pane()->running_view() != view_key(v))
        load_pane()->cancel();
    // Per view: its skeleton, and how old its remembered result may get before a quiet refresh — the
    // cycle dashboard and the curve follow the markets (30 min); the risk-premium estimate rests on
    // five- and ten-year history and moves with the daily close (6 h).
    set_view_load_policy(view_key(kCycle), ui::LoadingSkeleton::Shape::Cards, 30 * 60);
    set_view_load_policy(view_key(kErp), ui::LoadingSkeleton::Shape::Result, 6 * 3600);
    set_view_load_policy(view_key(kCurve), ui::LoadingSkeleton::Shape::Chart, 30 * 60);
    set_load_view(view_key(v));
}

void MarketModelsPanel::ensure_page(int view) {
    bool built = false;
    if (view == kCycle && !cycle_root_) {
        build_cycle_page();
        built = true;
    } else if (view == kErp && !erp_root_) {
        build_erp_page();
        built = true;
    } else if (view == kCurve && !curve_root_) {
        build_curve_page();
        built = true;
    }
    if (built) {
        apply_texts();
        apply_saved_inputs(view);
    }
}

void MarketModelsPanel::on_query_cancelled() {
    // The scripts were killed and their answers will not arrive: no view may stay "loading".
    for (Tab& t : tabs_) {
        if (t.state != TabState::Loading)
            continue;
        t.state = t.rows.isEmpty() ? TabState::Idle : TabState::Ready;
        t.pending.clear();
        t.message.clear();
    }
    fi_pending_.clear();
    curve_run_active_ = false;
    if (refresh_btn_)
        refresh_btn_->setEnabled(true);
}

QJsonObject MarketModelsPanel::curve_run_json(const CurveRun& run) {
    QJsonArray m;
    QJsonArray y;
    for (double v : run.m)
        m.append(v);
    for (double v : run.y)
        y.append(v);
    QJsonObject data;
    for (auto it = run.data.cbegin(); it != run.data.cend(); ++it)
        data.insert(it.key(), it.value());
    QJsonObject errors;
    for (auto it = run.errors.cbegin(); it != run.errors.cend(); ++it)
        errors.insert(it.key(), it.value());
    return QJsonObject{{QStringLiteral("m"), m},
                       {QStringLiteral("y"), y},
                       {QStringLiteral("freq"), run.freq},
                       {QStringLiteral("source"), run.source},
                       {QStringLiteral("as_of"), run.as_of},
                       {QStringLiteral("fetched_at"), run.fetched_at.toString(Qt::ISODate)},
                       {QStringLiteral("from_cache"), run.from_cache},
                       {QStringLiteral("note"), run.note},
                       {QStringLiteral("data"), data},
                       {QStringLiteral("errors"), errors}};
}

MarketModelsPanel::CurveRun MarketModelsPanel::curve_run_from_json(const QJsonObject& o) {
    CurveRun run;
    for (const QJsonValue v : o.value(QStringLiteral("m")).toArray())
        run.m.append(v.toDouble());
    for (const QJsonValue v : o.value(QStringLiteral("y")).toArray())
        run.y.append(v.toDouble());
    run.freq = o.value(QStringLiteral("freq")).toInt(2);
    run.source = o.value(QStringLiteral("source")).toString();
    run.as_of = o.value(QStringLiteral("as_of")).toString();
    run.fetched_at = QDateTime::fromString(o.value(QStringLiteral("fetched_at")).toString(), Qt::ISODate);
    run.from_cache = o.value(QStringLiteral("from_cache")).toBool();
    run.note = o.value(QStringLiteral("note")).toString();
    const QJsonObject data = o.value(QStringLiteral("data")).toObject();
    for (auto it = data.begin(); it != data.end(); ++it)
        run.data.insert(it.key(), it.value().toObject());
    const QJsonObject errors = o.value(QStringLiteral("errors")).toObject();
    for (auto it = errors.begin(); it != errors.end(); ++it)
        run.errors.insert(it.key(), it.value().toString());
    return run;
}

QJsonValue MarketModelsPanel::page_payload(int page) const {
    const int v = view_of_page(page);
    const Tab& t = tabs_[std::max(0, v)];
    QJsonObject o{{QStringLiteral("view"), view_key(v)},
                  {QStringLiteral("fetched_at"), t.fetched_at.toString(Qt::ISODate)},
                  {QStringLiteral("from_cache"), t.from_cache}};
    switch (v) {
        case kCycle:
            o.insert(QStringLiteral("data"), cycle_data_);
            break;
        case kErp:
            o.insert(QStringLiteral("data"), erp_data_);
            break;
        case kCurve:
            if (!curve_has_results_)
                return QJsonValue(); // the editor alone: nothing worth remembering
            o.insert(QStringLiteral("run"), curve_run_json(shown_run_));
            break;
        default:
            return QJsonValue();
    }
    return o;
}

bool MarketModelsPanel::restore_page(int /*page*/, const QJsonValue& payload) {
    // The page index of an earlier session can differ (pages are built in the order they are first
    // needed): the view is named in the payload.
    const QJsonObject o = payload.toObject();
    const QString key = o.value(QStringLiteral("view")).toString();
    int v = -1;
    for (int i = 0; i < kViewCount; ++i)
        if (view_key(i) == key)
            v = i;
    if (v < 0)
        return false;
    ensure_page(v);
    Tab& t = tabs_[v];
    t.fetched_at = QDateTime::fromString(o.value(QStringLiteral("fetched_at")).toString(), Qt::ISODate);
    t.from_cache = o.value(QStringLiteral("from_cache")).toBool();
    if (v == kCycle) {
        cycle_data_ = o.value(QStringLiteral("data")).toObject();
        if (cycle_data_.isEmpty())
            return false;
        populate_cycle();
    } else if (v == kErp) {
        erp_data_ = o.value(QStringLiteral("data")).toObject();
        if (erp_data_.isEmpty())
            return false;
        populate_erp();
    } else {
        shown_run_ = curve_run_from_json(o.value(QStringLiteral("run")).toObject());
        if (shown_run_.m.size() < 2)
            return false;
        if (curve_grid_ && curve_grid_->valid_row_count() == 0) {
            // The editor shows the curve the result was computed from.
            QJsonArray rows;
            for (int i = 0; i < shown_run_.m.size() && i < shown_run_.y.size(); ++i)
                rows.append(QJsonObject{{QStringLiteral("maturity"), shown_run_.m.at(i)},
                                        {QStringLiteral("yield"), shown_run_.y.at(i) * 100.0}});
            fill_curve_grid(rows);
            curve_source_ = shown_run_.source;
            curve_as_of_ = shown_run_.as_of;
            curve_fetched_at_ = shown_run_.fetched_at;
            curve_from_cache_ = shown_run_.from_cache;
            update_curve_source_label();
        }
        populate_curve();
        curve_msg_->setText(shown_run_.note);
        mmd::set_role(curve_msg_, "mmWarn");
        curve_msg_->setVisible(!shown_run_.note.isEmpty());
    }
    if (t.state != TabState::Loading) {
        t.state = TabState::Ready;
        t.message.clear();
    }
    publish_rows(v);
    show_content_page(t.page);
    return true;
}

void MarketModelsPanel::build_controls(QHBoxLayout* thl) {
    view_lbl_ = new QLabel;
    view_lbl_->setObjectName(QStringLiteral("econCtrlLabel"));

    view_combo_ = new QComboBox;
    for (const char* id : {"cycle", "erp", "curve"})
        view_combo_->addItem(QString(), QString::fromLatin1(id)); // texts: apply_texts()
    view_combo_->setMinimumHeight(26);
    view_combo_->setMinimumWidth(210);
    connect(view_combo_, &QComboBox::currentIndexChanged, this, [this]() {
        if (!restoring_)
            sync_load_view();
    });

    refresh_btn_ = new QPushButton;
    refresh_btn_->setObjectName(QStringLiteral("econCsvBtn"));
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() {
        emit query_started();
        force_next_ = true; // fresh market data instead of an answer from the last 10 minutes
        run_query();        // supersedes a load in flight
    });

    thl->addWidget(view_lbl_);
    thl->addWidget(view_combo_);
    thl->addSpacing(6);
    thl->addWidget(refresh_btn_);
}

void MarketModelsPanel::build_cycle_page() {
    cycle_root_ = new QWidget(content_host());
    cycle_root_->setObjectName(QStringLiteral("mmPage"));
    cycle_root_->setStyleSheet(page_style()); // once, before the children exist
    auto* outer = new QVBoxLayout(cycle_root_);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(cycle_root_);
    scroll->setObjectName(QStringLiteral("mmScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto* body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("mmBody"));
    auto* vl = new QVBoxLayout(body);
    vl->setContentsMargins(14, 10, 14, 14);
    vl->setSpacing(6);

    cycle_head_ = mmd::make_label(body, "mmHead");
    cycle_warn_ = mmd::make_label(body, "mmWarn");
    cycle_warn_->hide();
    vl->addWidget(cycle_head_);
    vl->addWidget(cycle_warn_);

    auto section = [&](QLabel*& lbl, ui::KpiStrip* strip) {
        lbl = mmd::make_label(body, "mmSection", false);
        vl->addSpacing(6);
        vl->addWidget(lbl);
        vl->addWidget(strip);
    };
    cycle_rates_ = mmd::make_strip(mmd::kCycleRates, body);
    cycle_equity_ = mmd::make_strip(mmd::kCycleEquity, body);
    cycle_cmdty_ = mmd::make_strip(mmd::kCycleCmdty, body);
    section(cycle_rates_lbl_, cycle_rates_);
    section(cycle_equity_lbl_, cycle_equity_);
    section(cycle_cmdty_lbl_, cycle_cmdty_);
    vl->addStretch(1);

    scroll->setWidget(body);
    outer->addWidget(scroll);
    tabs_[kCycle].page = add_content_page(cycle_root_);
}

void MarketModelsPanel::build_erp_page() {
    erp_root_ = new QWidget(content_host());
    erp_root_->setObjectName(QStringLiteral("mmPage"));
    erp_root_->setStyleSheet(page_style()); // once, before the children exist
    auto* outer = new QVBoxLayout(erp_root_);
    outer->setContentsMargins(0, 0, 0, 0);
    erp_scroll_ = new QScrollArea(erp_root_);
    erp_scroll_->setObjectName(QStringLiteral("mmScroll"));
    erp_scroll_->setFrameShape(QFrame::NoFrame);
    erp_scroll_->setWidgetResizable(true);
    auto* body = new QWidget(erp_scroll_);
    body->setObjectName(QStringLiteral("mmBody"));
    auto* vl = new QVBoxLayout(body);
    vl->setContentsMargins(14, 10, 14, 14);
    vl->setSpacing(8);

    // Inputs: blank = the script fetches the default. Typed in percent, sent as decimals.
    auto* box = new QFrame(body);
    box->setObjectName(QStringLiteral("mmInputs"));
    auto* bl = new QVBoxLayout(box);
    bl->setContentsMargins(12, 10, 12, 10);
    bl->setSpacing(6);
    erp_inputs_title_ = mmd::make_label(box, "mmSection", false);
    bl->addWidget(erp_inputs_title_);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(3);
    for (int i = 0; i <= kErpFieldCount; ++i) {
        auto* lbl = mmd::make_label(box, "mmFieldLbl", false);
        auto* edit = new QLineEdit(box);
        edit->setMinimumHeight(26);
        edit->setMinimumWidth(110);
        edit->setClearButtonEnabled(true);
        connect(edit, &QLineEdit::returnPressed, this, &MarketModelsPanel::run_erp_from_form);
        const int row = (i / 4) * 2;
        const int col = i % 4;
        grid->addWidget(lbl, row, col);
        grid->addWidget(edit, row + 1, col);
        erp_field_lbls_[i] = lbl;
        if (i < kErpFieldCount)
            erp_edits_[i] = edit;
        else
            erp_windows_ = edit;
    }
    auto* btns = new QHBoxLayout;
    btns->setSpacing(6);
    erp_run_btn_ = new QPushButton(box);
    erp_run_btn_->setObjectName(QStringLiteral("econActionBtn"));
    erp_run_btn_->setCursor(Qt::PointingHandCursor);
    connect(erp_run_btn_, &QPushButton::clicked, this, &MarketModelsPanel::run_erp_from_form);
    erp_clear_btn_ = new QPushButton(box);
    erp_clear_btn_->setObjectName(QStringLiteral("econCsvBtn"));
    erp_clear_btn_->setCursor(Qt::PointingHandCursor);
    connect(erp_clear_btn_, &QPushButton::clicked, this, [this]() {
        for (QLineEdit* e : erp_edits_)
            e->clear();
        erp_windows_->clear();
        erp_msg_->hide();
    });
    btns->addWidget(erp_run_btn_);
    btns->addWidget(erp_clear_btn_);
    btns->addStretch(1);
    grid->addLayout(btns, 3, 3, Qt::AlignBottom);
    for (int c = 0; c < 4; ++c)
        grid->setColumnStretch(c, 1);
    bl->addLayout(grid);
    erp_msg_ = mmd::make_label(box, "mmNote");
    erp_msg_->hide();
    bl->addWidget(erp_msg_);
    vl->addWidget(box);

    erp_kpis_ = mmd::make_strip(mmd::kErpTiles, body);
    vl->addWidget(erp_kpis_);
    erp_consensus_note_ = mmd::make_label(body, "mmNote");
    vl->addWidget(erp_consensus_note_);

    erp_methods_lbl_ = mmd::make_label(body, "mmSection", false);
    vl->addSpacing(4);
    vl->addWidget(erp_methods_lbl_);
    auto* cards = new QWidget(body);
    erp_cards_ = new QVBoxLayout(cards);
    erp_cards_->setContentsMargins(0, 0, 0, 0);
    erp_cards_->setSpacing(8);
    vl->addWidget(cards);

    erp_sources_lbl_ = mmd::make_label(body, "mmSection", false);
    vl->addSpacing(4);
    vl->addWidget(erp_sources_lbl_);
    erp_inputs_table_ = mmd::make_table(body);
    erp_inputs_table_->setColumnCount(4);
    erp_inputs_table_->setWordWrap(true);
    erp_inputs_table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    erp_inputs_table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Interactive columns sized once after each fill (never resize-to-contents while rows are filled).
    erp_inputs_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    erp_inputs_table_->horizontalHeader()->setStretchLastSection(true);
    // Source texts wrap; re-measure the rows whenever the SOURCE column changes width.
    connect(erp_inputs_table_->horizontalHeader(), &QHeaderView::sectionResized, this,
            [this]() { fit_erp_inputs_table(); });
    vl->addWidget(erp_inputs_table_);
    erp_notes_ = mmd::make_label(body, "mmNote");
    erp_notes_->hide();
    vl->addWidget(erp_notes_);
    vl->addStretch(1);

    erp_scroll_->setWidget(body);
    outer->addWidget(erp_scroll_);
    tabs_[kErp].page = add_content_page(erp_root_);
}

void MarketModelsPanel::build_curve_page() {
    curve_root_ = new QWidget(content_host());
    curve_root_->setObjectName(QStringLiteral("mmPage"));
    curve_root_->setStyleSheet(page_style()); // once, before the children exist
    auto* outer = new QHBoxLayout(curve_root_);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* split = new QSplitter(Qt::Horizontal, curve_root_);
    split->setObjectName(QStringLiteral("mmSplit"));
    split->setHandleWidth(3);
    split->setChildrenCollapsible(false);

    // Left: results — tiles, notes, chart above the maturity table.
    auto* left = new QWidget(split);
    left->setObjectName(QStringLiteral("mmBody"));
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(12, 8, 10, 8);
    lvl->setSpacing(6);
    curve_kpis_ = mmd::make_strip(mmd::kCurveTiles, left);
    lvl->addWidget(curve_kpis_);
    curve_caption_ = mmd::make_label(left, "mmNote");
    lvl->addWidget(curve_caption_);
    auto* vsplit = new QSplitter(Qt::Vertical, left);
    vsplit->setObjectName(QStringLiteral("mmSplit"));
    vsplit->setHandleWidth(3);
    vsplit->setChildrenCollapsible(false);
    curve_chart_ = new ui::TimeSeriesChart(vsplit);
    curve_chart_->set_x_is_time(false);
    curve_chart_->set_show_mode_buttons(false);
    curve_chart_->set_zoom_enabled(false);
    curve_chart_->set_show_legend(true);
    curve_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left,
                                      [](double v) { return mmd::fmt::pct(v, 2, false); });
    curve_chart_->set_tick_time_formatter([](double x) { return mmd::tenor_label(x * x); });
    curve_chart_->set_time_formatter([](double x) { return mmd::tenor_label(x * x); });
    curve_chart_->set_export_name(QStringLiteral("yield_curve_analytics"));
    curve_chart_->setMinimumHeight(200);
    curve_table_ = mmd::make_table(vsplit);
    curve_table_->setMinimumHeight(110);
    vsplit->addWidget(curve_chart_);
    vsplit->addWidget(curve_table_);
    vsplit->setStretchFactor(0, 3);
    vsplit->setStretchFactor(1, 2);
    lvl->addWidget(vsplit, 1);

    // Right: the input curve — editable, pasteable, reloadable.
    auto* right = new QFrame(split);
    right->setObjectName(QStringLiteral("mmEditor"));
    right->setMinimumWidth(270);
    auto* rvl = new QVBoxLayout(right);
    rvl->setContentsMargins(10, 8, 10, 8);
    rvl->setSpacing(6);
    curve_editor_title_ = mmd::make_label(right, "mmSection", false);
    rvl->addWidget(curve_editor_title_);
    curve_src_lbl_ = mmd::make_label(right, "mmNote");
    rvl->addWidget(curve_src_lbl_);

    // Column titles are fixed at construction (DataGridEditor has no setter for them).
    auto mat_col = ui::DataGridColumn::number(QStringLiteral("maturity"), tr("MATURITY (YRS)"), 4);
    mat_col.tooltip = tr("Years to maturity: 0.25 = 3 months. Pasted tenors such as 3M or 10Y are converted.");
    auto yld_col = ui::DataGridColumn::number(QStringLiteral("yield"), tr("YIELD (%)"), 3);
    yld_col.tooltip = tr("Par yield in percent: 4.25 means 4.25 %.");
    curve_grid_ = new ui::DataGridEditor({mat_col, yld_col}, right);
    curve_grid_->set_paste_adapter(&mmd::adapt_curve_paste);
    curve_grid_->set_visible_rows(12);
    connect(curve_grid_, &ui::DataGridEditor::changed, this, [this]() {
        if (filling_grid_)
            return;
        curve_source_ = QString::fromLatin1(mmd::kSrcManual); // typed, pasted or edited
        curve_as_of_.clear();
        update_curve_source_label();
    });
    rvl->addWidget(curve_grid_, 1);

    auto* freq_row = new QHBoxLayout;
    freq_row->setSpacing(6);
    curve_freq_lbl_ = mmd::make_label(right, "mmFieldLbl", false);
    curve_freq_combo_ = new QComboBox(right);
    for (int f : {2, 1, 4, 0})
        curve_freq_combo_->addItem(QString(), f); // texts: apply_texts()
    curve_freq_combo_->setMinimumHeight(26);
    freq_row->addWidget(curve_freq_lbl_);
    freq_row->addWidget(curve_freq_combo_, 1);
    rvl->addLayout(freq_row);

    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(6);
    curve_load_btn_ = new QPushButton(right);
    curve_load_btn_->setObjectName(QStringLiteral("econCsvBtn"));
    curve_load_btn_->setCursor(Qt::PointingHandCursor);
    connect(curve_load_btn_, &QPushButton::clicked, this, [this]() {
        emit query_started();
        curve_source_.clear(); // FETCH on an empty source loads the Treasury curve
        filling_grid_ = true;
        curve_grid_->from_json(QJsonArray());
        filling_grid_ = false;
        run_query();
    });
    curve_run_btn_ = new QPushButton(right);
    curve_run_btn_->setObjectName(QStringLiteral("econActionBtn"));
    curve_run_btn_->setCursor(Qt::PointingHandCursor);
    connect(curve_run_btn_, &QPushButton::clicked, this, &MarketModelsPanel::run_curve_from_editor);
    btn_row->addWidget(curve_load_btn_, 1);
    btn_row->addWidget(curve_run_btn_, 1);
    rvl->addLayout(btn_row);
    curve_msg_ = mmd::make_label(right, "mmNote");
    curve_msg_->hide();
    rvl->addWidget(curve_msg_);

    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    outer->addWidget(split);
    tabs_[kCurve].page = add_content_page(curve_root_);
}

// ── Texts / theme ─────────────────────────────────────────────────────────────

void MarketModelsPanel::apply_texts() {
    if (view_lbl_)
        view_lbl_->setText(tr("VIEW"));
    if (view_combo_ && view_combo_->count() == kViewCount) {
        view_combo_->setItemText(kCycle, tr("Business cycle"));
        view_combo_->setItemText(kErp, tr("Equity risk premium"));
        view_combo_->setItemText(kCurve, tr("Yield curve analytics"));
        view_combo_->setItemData(kCycle,
                                 tr("S&P 500, VIX, Treasuries, gold, oil and the dollar index with curve spreads "
                                    "and S&P 500 trend statistics (Yahoo Finance, no key)"),
                                 Qt::ToolTipRole);
        view_combo_->setItemData(kErp,
                                 tr("Equity risk premium of the S&P 500 over the 10Y Treasury: realised, implied "
                                    "from the earnings yield and supply side"),
                                 Qt::ToolTipRole);
        view_combo_->setItemData(kCurve,
                                 tr("Shape, spreads, bootstrapped zero curve, forwards and a Nelson–Siegel fit of a "
                                    "par curve — the latest US Treasury curve or one you enter"),
                                 Qt::ToolTipRole);
        view_combo_->setAccessibleName(tr("Market model view"));
    }
    if (refresh_btn_) {
        refresh_btn_->setText(tr("REFRESH"));
        refresh_btn_->setToolTip(tr("Download fresh market data for this view instead of reusing an answer from the "
                                    "last 10 minutes (a custom curve is simply analysed again)"));
    }
    // Business cycle
    if (cycle_root_) {
        cycle_rates_lbl_->setText(tr("RATES & CURVE"));
        cycle_equity_lbl_->setText(tr("EQUITIES"));
        cycle_cmdty_lbl_->setText(tr("COMMODITIES & DOLLAR"));
        mmd::relabel(cycle_rates_, mmd::kCycleRates);
        mmd::relabel(cycle_equity_, mmd::kCycleEquity);
        mmd::relabel(cycle_cmdty_, mmd::kCycleCmdty);
    }
    if (erp_root_)
        apply_erp_texts();
    if (curve_root_)
        apply_curve_texts();
}

void MarketModelsPanel::apply_erp_texts() {
    erp_inputs_title_->setText(tr("INPUTS — LEAVE BLANK TO USE THE FETCHED DEFAULT"));
    for (int i = 0; i < kErpFieldCount; ++i) {
        const mmd::ErpField& f = mmd::kErpFields[i];
        erp_field_lbls_[i]->setText(tr(f.label));
        erp_edits_[i]->setPlaceholderText(tr(f.placeholder));
        erp_edits_[i]->setToolTip(tr(f.tip));
        erp_edits_[i]->setAccessibleName(tr(f.label));
    }
    erp_field_lbls_[kErpFieldCount]->setText(tr("WINDOWS (YEARS)"));
    erp_windows_->setPlaceholderText(tr("5, 10"));
    erp_windows_->setToolTip(tr("Historical windows in whole years (1–30), separated by commas. The longest one "
                                "enters the consensus."));
    erp_windows_->setAccessibleName(tr("Historical windows in years"));
    erp_run_btn_->setText(tr("ESTIMATE"));
    erp_run_btn_->setToolTip(tr("Estimate the equity risk premium with these inputs (Enter in any field)"));
    erp_clear_btn_->setText(tr("CLEAR"));
    erp_clear_btn_->setToolTip(tr("Clear every override and use the fetched defaults"));
    mmd::relabel(erp_kpis_, mmd::kErpTiles);
    erp_methods_lbl_->setText(tr("ESTIMATES BY METHOD"));
    erp_sources_lbl_->setText(tr("INPUTS USED — VALUE, DATE AND SOURCE"));
    erp_inputs_table_->setHorizontalHeaderLabels({tr("INPUT"), tr("VALUE"), tr("AS OF"), tr("SOURCE")});
    erp_inputs_table_->setAccessibleName(tr("Equity risk premium inputs and their sources"));
    if (erp_data_.isEmpty())
        erp_consensus_note_->setText(tr("No estimate yet — adjust the inputs and press ESTIMATE (or Enter)."));
}

void MarketModelsPanel::apply_curve_texts() {
    mmd::relabel(curve_kpis_, mmd::kCurveTiles);
    curve_editor_title_->setText(tr("INPUT CURVE"));
    curve_grid_->set_hint(tr("paste two columns: maturity (years, or 3M / 10Y) and yield (%)"));
    curve_grid_->setAccessibleName(tr("Input curve: maturity in years and par yield in percent"));
    curve_freq_lbl_->setText(tr("COUPONS"));
    curve_freq_combo_->setItemText(0, tr("Semiannual (2 a year)"));
    curve_freq_combo_->setItemText(1, tr("Annual (1 a year)"));
    curve_freq_combo_->setItemText(2, tr("Quarterly (4 a year)"));
    curve_freq_combo_->setItemText(3, tr("None — zero-coupon rates"));
    curve_freq_combo_->setToolTip(tr("Coupon frequency of the par bonds behind the curve, used by the bootstrap: "
                                     "US Treasuries and gilts pay twice a year, Bunds and OATs once."));
    curve_load_btn_->setText(tr("LOAD US TREASURY"));
    curve_load_btn_->setToolTip(tr("Replace the input with the latest US Treasury par yield curve "
                                   "(Federal Reserve H.15, no API key) and analyse it"));
    curve_run_btn_->setText(tr("RUN"));
    curve_run_btn_->setToolTip(tr("Analyse the input curve: shape, spreads, zero and forward curves, "
                                  "Nelson–Siegel fit"));
    curve_chart_->setAccessibleName(tr("Yield curve chart: par, zero, forward and fitted curves"));
    curve_table_->setAccessibleName(tr("Yield curve by maturity"));
    if (!curve_has_results_) {
        curve_table_->setColumnCount(0);
        clear_curve_results(tr("Enter or paste a curve on the right — maturity in years, yield in percent — "
                               "then press RUN, or LOAD US TREASURY."));
    }
    update_curve_source_label();
}

QString MarketModelsPanel::page_style() const {
    using namespace ui::colors;
    const auto c = [](const ui::ColorToken& t) { return QString::fromLatin1(t.get()); };
    // The provider's identity colour, made legible on the active theme (as EconPanelBase does).
    const QColor accent = legible_color(QColor(color_));
    return QString(
               "#mmPage, #mmBody { background:%1; }"
               "QScrollArea#mmScroll { background:%1; border:none; }"
               "QScrollArea#mmScroll > QWidget#qt_scrollarea_viewport { background:%1; }"
               "QLabel#mmHead { color:%2; font-size:" + ui::fonts::px(-1) + "px; font-weight:700;"
               "  background:transparent; }"
               "QLabel#mmSection { color:%3; font-size:" + ui::fonts::px(-5) + "px; font-weight:700;"
               "  letter-spacing:1px; background:transparent; }"
               "QLabel#mmFieldLbl { color:%3; font-size:" + ui::fonts::px(-6) + "px; font-weight:700;"
               "  letter-spacing:0.5px; background:transparent; }"
               "QLabel#mmNote { color:%4; font-size:" + ui::fonts::px(-4) + "px; background:transparent; }"
               "QLabel#mmWarn { color:%5; font-size:" + ui::fonts::px(-4) + "px; background:transparent; }"
               "QLabel#mmError { color:%6; font-size:" + ui::fonts::px(-4) + "px; background:transparent; }"
               "QFrame#mmCard, QFrame#mmInputs { background:%7; border:1px solid %8; }"
               "QFrame#mmEditor { background:%7; border:none; border-left:1px solid %8; }"
               "QLabel#mmCardTitle { color:%3; font-size:" + ui::fonts::px(-5) + "px; font-weight:700;"
               "  letter-spacing:1px; background:transparent; }"
               "QLabel#mmCardValue { color:%9; font-size:" + ui::fonts::px(3) + "px; font-weight:700;"
               "  background:transparent; }"
               "QLabel#mmCardValueDim { color:%4; font-size:" + ui::fonts::px(3) + "px; font-weight:700;"
               "  background:transparent; }"
               "QLabel#mmCardBody { color:%2; font-size:" + ui::fonts::px(-3) + "px; background:transparent; }"
               "QSplitter#mmSplit::handle { background:%8; }")
        .arg(c(BG_BASE), c(TEXT_PRIMARY), c(TEXT_SECONDARY), c(TEXT_TERTIARY), c(WARNING), c(DANGER), c(BG_SURFACE),
             c(BORDER_DIM), accent.name());
}

void MarketModelsPanel::refresh_panel_theme() {
    // The pages' stylesheets were set once, before their children existed; ThemeManager's re-style
    // pass keeps them in step. Tiles and charts paint from the live theme.
    EconPanelBase::refresh_panel_theme();
}

void MarketModelsPanel::retranslateUi() {
    apply_texts();
    // Card, tile and table texts are partly data-derived: rebuild them in the new language.
    if (cycle_root_ && !cycle_data_.isEmpty())
        populate_cycle();
    if (erp_root_ && !erp_data_.isEmpty())
        populate_erp();
    if (curve_root_ && curve_has_results_)
        populate_curve();
    EconPanelBase::retranslateUi();
}

// ── View state ────────────────────────────────────────────────────────────────

void MarketModelsPanel::activate() {
    // Nothing to prepare: the results pane shows the view's remembered result or loads it on show.
}

int MarketModelsPanel::view_for_id(const QString& id) {
    const QString k = id.trimmed().toLower();
    if (k == QLatin1String("cycle") || k == QLatin1String("business_cycle"))
        return kCycle;
    if (k == QLatin1String("erp") || k == QLatin1String("equity_risk_premium"))
        return kErp;
    if (k == QLatin1String("curve") || k == QLatin1String("yield_curve"))
        return kCurve;
    return -1;
}

bool MarketModelsPanel::select_view(const QString& id) {
    const int v = view_for_id(id);
    if (v < 0 || !view_combo_)
        return false;
    view_combo_->setCurrentIndex(v); // currentIndexChanged -> sync_load_view(), as the VIEW box
    return true;
}

int MarketModelsPanel::current_view() const {
    if (!view_combo_)
        return kCycle;
    return std::clamp(view_combo_->currentIndex(), 0, static_cast<int>(kViewCount) - 1);
}

void MarketModelsPanel::render_view() {
    const int v = current_view();
    Tab& t = tabs_[v];
    if (t.state != TabState::Ready)
        return;
    ensure_page(v);
    publish_rows(v);
    show_content_page(t.page); // the view's result: delivered to (and remembered by) the results pane
}

void MarketModelsPanel::publish_rows(int v) {
    Tab& t = tabs_[v];
    if (t.rows.isEmpty()) {
        // Nothing of this view's to export yet — do not let CSV write another view's rows.
        set_banner({});
        if (export_btn_)
            export_btn_->setEnabled(false);
        return;
    }
    set_retrieval_info(t.fetched_at, t.from_cache);
    set_series_meta(t.meta);
    display(t.rows, t.title, t.columns, /*keep_row_order=*/true);
    set_banner(t.banner); // after display(), which clears it
    if (export_btn_)
        export_btn_->setEnabled(true);
}

void MarketModelsPanel::show_view_error(int v) {
    const QString msg = tabs_[v].message;
    if (v == kErp) {
        show_error_with_action(msg, tr("EDIT INPUTS"), [this]() {
            show_erp_inputs(tr("Last run failed: %1").arg(tabs_[kErp].message), true);
        });
    } else if (v == kCurve) {
        const bool has_curve = curve_grid_ && curve_grid_->valid_row_count() > 0;
        show_error_with_action(msg, has_curve ? tr("EDIT CURVE") : tr("ENTER CURVE"),
                               [this]() { show_curve_editor(tabs_[kCurve].message, true); });
    } else {
        show_error(msg);
    }
}

void MarketModelsPanel::set_loading(int v, const QString& message, const QString& request_id) {
    Tab& t = tabs_[v];
    t.state = TabState::Loading;
    t.message = message;
    t.pending = request_id;
    if (v == current_view())
        show_loading(message); // the skeleton's caption (the result on screen, if any, stays)
}

void MarketModelsPanel::set_error(int v, const QString& message) {
    Tab& t = tabs_[v];
    t.state = TabState::Error;
    t.message = message;
    t.pending.clear();
    LOG_WARN("MarketModelsPanel", logf("Market model view failed", {{"view", view_key(v)}, {"error", message.left(300)}}));
    if (v == current_view())
        show_view_error(v);
}

void MarketModelsPanel::run_view(int v, bool force) {
    switch (v) {
        case kCycle:
            start_cycle(force);
            break;
        case kErp:
            start_erp(force);
            break;
        case kCurve:
            // REFRESH re-downloads the Treasury curve, but never replaces a curve the user entered (LOAD US
            // TREASURY does that explicitly). FETCH analyses what is in the editor — a custom curve with
            // problems goes back to the editor with the reason — or loads the Treasury curve when there is none.
            if (force && curve_source_ != QLatin1String(mmd::kSrcManual))
                load_treasury(true);
            else if (curve_source_ == QLatin1String(mmd::kSrcManual) ||
                     (curve_grid_ && curve_grid_->valid_row_count() >= 2))
                run_curve();
            else
                load_treasury(force);
            break;
        default:
            break;
    }
}

void MarketModelsPanel::run_script(const QString& script, const QString& command, const QString& args,
                                   const QString& request_id, bool force) {
    const QStringList argv = args.isEmpty() ? QStringList{} : QStringList{args};
    // Tracked by the results pane's run: leaving the view during the automatic load kills the script.
    // `force` (REFRESH) drops the cached answer so the script runs again.
    execute_query(QString::fromLatin1(mmd::kSourceId), script, command, argv, request_id, force);
}

QString MarketModelsPanel::error_of(const services::EconomicsResult& r) const {
    if (!r.success)
        return r.error.trimmed().isEmpty() ? tr("The script failed without a message.") : r.error.trimmed();
    const QJsonValue e = r.data.value(QStringLiteral("error"));
    if (e.isString() && !e.toString().trimmed().isEmpty())
        return e.toString().trimmed();
    return {};
}

void MarketModelsPanel::on_fetch() {
    const int v = current_view();
    ensure_page(v); // the view's inputs (ERP overrides, the curve editor) live on its page
    const bool force = std::exchange(force_next_, false);
    // An automatic load (first show, or a quiet refresh of a result older than 30 minutes) analyses
    // the latest Treasury curve, not the one left in the editor by an earlier load; a curve the user
    // entered is analysed as it is.
    if (v == kCurve && query_is_automatic() && curve_source_ != QLatin1String(mmd::kSrcManual)) {
        load_treasury(force);
        return;
    }
    run_view(v, force);
}

void MarketModelsPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != QLatin1String(mmd::kSourceId) || request_id.isEmpty())
        return;
    if (request_id == tabs_[kCycle].pending) {
        on_cycle_result(result);
    } else if (request_id == tabs_[kErp].pending) {
        on_erp_result(result);
    } else if (request_id == tabs_[kCurve].pending) {
        on_treasury_result(result);
    } else {
        QString command;
        for (auto it = fi_pending_.cbegin(); it != fi_pending_.cend(); ++it) {
            if (it.value() == request_id) {
                command = it.key();
                break;
            }
        }
        if (!command.isEmpty())
            on_fi_result(command, result);
        // Anything else answers a request this panel has since replaced.
    }
    // The base records the retrieval time of every result of this source; keep the provenance line on
    // the visible view's data.
    const Tab& t = tabs_[current_view()];
    set_retrieval_info(t.fetched_at, t.from_cache);
}

// ── Business cycle ────────────────────────────────────────────────────────────

void MarketModelsPanel::start_cycle(bool force) {
    const QString args = mmd::json_text(QJsonObject{{QStringLiteral("timeout"), mmd::kNetTimeoutSec}});
    set_loading(kCycle,
                tr("Loading business-cycle indicators — S&P 500, VIX, Treasuries, gold, oil and the dollar "
                   "(Yahoo Finance)…"),
                QString::fromLatin1(mmd::kCycleRequest));
    run_script(QString::fromLatin1(mmd::kCycleScript), QStringLiteral("get_indicators"), args,
               QString::fromLatin1(mmd::kCycleRequest), force);
}

void MarketModelsPanel::on_cycle_result(const services::EconomicsResult& r) {
    Tab& t = tabs_[kCycle];
    t.pending.clear();
    const QString err = error_of(r);
    if (!err.isEmpty()) {
        set_error(kCycle, err);
        return;
    }
    cycle_data_ = r.data;
    t.fetched_at = r.fetched_at;
    t.from_cache = r.from_cache;
    t.state = TabState::Ready;
    t.message.clear();
    ensure_page(kCycle);
    populate_cycle();
    LOG_DEBUG("MarketModelsPanel", "Business cycle indicators as of " + mmd::str(cycle_data_, "as_of"));
    if (current_view() == kCycle)
        render_view();
}

void MarketModelsPanel::populate_cycle() {
    using ui::StatTile;
    const QJsonObject& d = cycle_data_;
    const QJsonObject sources = d.value(QStringLiteral("sources")).toObject();

    // name → why it is missing
    QHash<QString, QString> missing;
    QStringList missing_text;
    for (const QJsonValue& u : d.value(QStringLiteral("unavailable")).toArray()) {
        const QJsonObject o = u.toObject();
        const QString name = u.isString() ? u.toString() : mmd::str(o, "name");
        if (name.isEmpty())
            continue;
        const QString ticker = mmd::str(o, "ticker");
        const QString reason = mmd::str(o, "reason");
        missing.insert(name, reason);
        QString line = ticker.isEmpty() ? name : tr("%1 (%2)").arg(name, ticker);
        if (!reason.isEmpty())
            line += QStringLiteral(": ") + reason;
        missing_text << line;
    }

    auto tip_for = [&](const QString& name, const QString& what = {}) {
        QStringList lines;
        if (!what.isEmpty())
            lines << what;
        const QJsonObject s = sources.value(name).toObject();
        if (!s.isEmpty()) {
            lines << tr("%1 · %2").arg(mmd::str(s, "ticker"), mmd::str(s, "provider"));
            if (!mmd::str(s, "unit").isEmpty())
                lines << tr("Unit: %1").arg(mmd::str(s, "unit"));
            if (!mmd::str(s, "as_of").isEmpty())
                lines << tr("As of %1").arg(mmd::str(s, "as_of"));
        }
        if (missing.contains(name))
            lines << tr("Unavailable: %1").arg(missing.value(name));
        return lines.join(QLatin1Char('\n'));
    };
    auto unit_of = [&](const QString& name) { return mmd::str(sources.value(name).toObject(), "unit"); };
    auto tile = [](ui::KpiStrip* strip, const char* key) { return strip->tile(QString::fromLatin1(key)); };
    // Value, delta (percent change, or basis points for yields) and tooltip of one market series.
    auto series = [&](StatTile* t, const QString& name, const QString& value, bool yield) {
        if (!t)
            return;
        t->set_value(value);
        t->set_tooltip_text(tip_for(name));
        const double chg_bp = mmd::num(d.value(name + QStringLiteral("_change_bp")));
        const double chg_pct = mmd::num(d.value(name + QStringLiteral("_change_pct")));
        if (yield && mmd::has(chg_bp))
            t->set_delta(chg_bp, mmd::fmt::bp(chg_bp, 1));
        else if (!yield && mmd::has(chg_pct))
            t->set_delta(chg_pct);
        if (!mmd::has(mmd::num(d.value(name))))
            t->set_sub_label(tr("unavailable"));
    };

    mmd::reset_tiles(cycle_rates_, mmd::kCycleRates);
    mmd::reset_tiles(cycle_equity_, mmd::kCycleEquity);
    mmd::reset_tiles(cycle_cmdty_, mmd::kCycleCmdty);

    // ── Rates & curve
    const QString t3m_name = QStringLiteral("treasury_3m_yield");
    if (StatTile* t = tile(cycle_rates_, "t3m")) {
        series(t, t3m_name, mmd::level_pct(mmd::num(d, "treasury_3m_yield")), true);
        const double disc = mmd::num(d, "treasury_3m_discount_rate");
        if (mmd::has(mmd::num(d, "treasury_3m_yield")))
            t->set_sub_label(mmd::has(disc) ? tr("bond-equivalent · discount %1").arg(mmd::level_pct(disc))
                                            : tr("bond-equivalent yield"));
    }
    series(tile(cycle_rates_, "t10"), QStringLiteral("treasury_10y_yield"),
           mmd::level_pct(mmd::num(d, "treasury_10y_yield")), true);
    series(tile(cycle_rates_, "t30"), QStringLiteral("treasury_30y_yield"),
           mmd::level_pct(mmd::num(d, "treasury_30y_yield")), true);
    // The script gives the spreads in percentage points; the tiles show basis points, as for every spread.
    if (StatTile* t = tile(cycle_rates_, "s10_3")) {
        const double spread_pp = mmd::num(d, "yield_curve_spread_10y_3m");
        t->set_value(mmd::bps(spread_pp * 100.0));
        const QJsonValue inv = d.value(QStringLiteral("yield_curve_inverted"));
        if (inv.isBool()) {
            t->set_sub_label(inv.toBool() ? tr("INVERTED — 10Y below 3M") : tr("not inverted"));
            if (inv.toBool())
                t->set_tone(StatTile::Tone::Warning);
        }
        t->set_tooltip_text(mmd::has(spread_pp) ? tr("10-year yield minus the 3-month bill yield: %1 percentage points")
                                                      .arg(mmd::fmt::signed_number(spread_pp, 2))
                                                : tr("Needs both the 10-year and the 3-month yield"));
    }
    if (StatTile* t = tile(cycle_rates_, "s30_10")) {
        const double spread_pp = mmd::num(d, "yield_curve_spread_30y_10y");
        t->set_value(mmd::bps(spread_pp * 100.0));
        t->set_sub_label(tr("long-end term spread"));
        t->set_tooltip_text(mmd::has(spread_pp) ? tr("30-year yield minus the 10-year yield: %1 percentage points")
                                                      .arg(mmd::fmt::signed_number(spread_pp, 2))
                                                : tr("Needs both the 30-year and the 10-year yield"));
    }

    // ── Equities
    const double spx = mmd::num(d, "sp500");
    series(tile(cycle_equity_, "spx"), QStringLiteral("sp500"), mmd::plain(spx), false);
    if (StatTile* t = tile(cycle_equity_, "vix")) {
        series(t, QStringLiteral("vix"), mmd::plain(mmd::num(d, "vix")), false);
        const QString mood = mmd::str(d, "market_sentiment");
        if (!mood.isEmpty())
            t->set_sub_label(mood);
        t->set_tooltip_text(tip_for(QStringLiteral("vix"),
                                    tr("Sentiment from the VIX level: below 15, 15–20, 20–30, 30 and above")));
    }
    const QJsonObject window = d.value(QStringLiteral("sp500_stats_window")).toObject();
    auto return_tile = [&](StatTile* t, double v, const QString& sub, const QString& tip) {
        if (!t)
            return;
        t->set_value(mmd::has(v) ? mmd::fmt::pct(v, 2, true) : mmd::fmt::placeholder());
        if (mmd::has(v) && v != 0.0)
            t->set_tone(v > 0 ? StatTile::Tone::Positive : StatTile::Tone::Negative); // price direction
        t->set_sub_label(sub);
        t->set_tooltip_text(tip);
    };
    const QString ytd_base = mmd::str(d, "sp500_ytd_base_date");
    return_tile(tile(cycle_equity_, "ytd"), mmd::num(d, "sp500_ytd_return_pct"),
                ytd_base.isEmpty() ? QString() : tr("since the %1 close").arg(ytd_base),
                tr("S&P 500 price return since the last close of the previous year"));
    return_tile(tile(cycle_equity_, "y1"), mmd::num(d, "sp500_1y_return_pct"),
                window.isEmpty() ? QString() : tr("since %1").arg(mmd::str(window, "from")),
                tr("S&P 500 price return over the last year (from the first close of the window)"));
    if (StatTile* t = tile(cycle_equity_, "range")) {
        const double lo = mmd::num(d, "sp500_52w_low");
        const double hi = mmd::num(d, "sp500_52w_high");
        if (mmd::has(lo) && mmd::has(hi)) {
            t->set_value(tr("%1 – %2").arg(mmd::plain(lo), mmd::plain(hi)));
            if (mmd::has(spx) && hi > lo)
                t->set_sub_label(tr("now at %1 of the range").arg(mmd::fmt::pct((spx - lo) / (hi - lo) * 100.0, 0, false)));
        }
        QString tip = tr("Lowest and highest daily close of the last year");
        if (!window.isEmpty())
            tip += QLatin1Char('\n') + tr("%1 → %2, %3 closes")
                                           .arg(mmd::str(window, "from"), mmd::str(window, "to"))
                                           .arg(static_cast<int>(mmd::num(window, "observations")));
        t->set_tooltip_text(tip);
    }
    if (StatTile* t = tile(cycle_equity_, "sma")) {
        const double s50 = mmd::num(d, "sp500_sma_50");
        const double s200 = mmd::num(d, "sp500_sma_200");
        if (mmd::has(s50) || mmd::has(s200))
            t->set_value(tr("%1 / %2").arg(mmd::plain(s50), mmd::plain(s200)));
        const QJsonValue above = d.value(QStringLiteral("sp500_above_sma200"));
        if (above.isBool())
            t->set_sub_label(above.toBool() ? tr("index above its 200-day average")
                                            : tr("index below its 200-day average"));
        t->set_tooltip_text(tr("Simple moving averages of the S&P 500's daily closes"));
    }

    // ── Commodities & dollar
    auto cmdty = [&](const char* key, const QString& name) {
        StatTile* t = tile(cycle_cmdty_, key);
        series(t, name, mmd::plain(mmd::num(d.value(name))), false);
        if (t && mmd::has(mmd::num(d.value(name))))
            t->set_sub_label(unit_of(name));
    };
    cmdty("gold", QStringLiteral("gold_price"));
    cmdty("oil", QStringLiteral("crude_oil_price"));
    cmdty("dxy", QStringLiteral("us_dollar_index"));
    if (StatTile* t = tile(cycle_cmdty_, "go")) {
        t->set_value(mmd::plain(mmd::num(d, "gold_oil_ratio")));
        t->set_sub_label(tr("barrels of WTI per ounce of gold"));
        t->set_tooltip_text(tr("Gold (USD per ounce) divided by WTI crude (USD per barrel)"));
    }

    // ── Header and the missing-series line
    const QString as_of = mmd::str(d, "as_of");
    const QString mood = mmd::str(d, "market_sentiment");
    QString head = tr("As of %1").arg(as_of.isEmpty() ? mmd::fmt::placeholder() : as_of);
    if (!mood.isEmpty())
        head += QStringLiteral("   ·   ") + tr("Market sentiment: %1").arg(mood);
    cycle_head_->setText(head);
    const QString warn = missing_text.isEmpty()
                             ? QString()
                             : tr("%n series unavailable — %1", "", static_cast<int>(missing_text.size()))
                                   .arg(missing_text.join(QStringLiteral("; ")));
    cycle_warn_->setText(warn);
    cycle_warn_->setVisible(!warn.isEmpty());

    // ── Rows for the title bar / CSV: one per series, then the derived figures.
    QJsonArray rows;
    for (const char* n : {"sp500", "vix", "treasury_3m_yield", "treasury_3m_discount_rate", "treasury_10y_yield",
                          "treasury_30y_yield", "gold_price", "crude_oil_price", "us_dollar_index"}) {
        const QString name = QString::fromLatin1(n);
        const QJsonObject s = sources.value(name).toObject();
        QJsonObject row{{QStringLiteral("indicator"), name},
                        {QStringLiteral("value"), d.value(name).isDouble() ? d.value(name) : QJsonValue()},
                        {QStringLiteral("change_pct"), mmd::json_num(mmd::num(d.value(name + QStringLiteral("_change_pct"))))},
                        {QStringLiteral("unit"), s.value(QStringLiteral("unit"))},
                        {QStringLiteral("ticker"), s.value(QStringLiteral("ticker"))},
                        {QStringLiteral("provider"), s.value(QStringLiteral("provider"))},
                        {QStringLiteral("as_of"), s.value(QStringLiteral("as_of"))}};
        if (d.contains(name + QStringLiteral("_change_bp")))
            row.insert(QStringLiteral("change_bp"), mmd::json_num(mmd::num(d.value(name + QStringLiteral("_change_bp")))));
        rows.append(row);
    }
    const struct {
        const char* key;
        const char* unit;
    } kDerived[] = {{"yield_curve_spread_10y_3m", "percentage points"},
                    {"yield_curve_spread_30y_10y", "percentage points"},
                    {"yield_curve_inverted", ""},
                    {"gold_oil_ratio", "barrels per ounce"},
                    {"sp500_52w_high", "index points"},
                    {"sp500_52w_low", "index points"},
                    {"sp500_sma_50", "index points"},
                    {"sp500_sma_200", "index points"},
                    {"sp500_above_sma200", ""},
                    {"sp500_ytd_return_pct", "percent"},
                    {"sp500_1y_return_pct", "percent"},
                    {"market_sentiment", ""}};
    for (const auto& dv : kDerived) {
        const QJsonValue v = d.value(QLatin1String(dv.key));
        rows.append(QJsonObject{{QStringLiteral("indicator"), QString::fromLatin1(dv.key)},
                                {QStringLiteral("value"), v.isUndefined() ? QJsonValue() : v},
                                {QStringLiteral("unit"), QString::fromLatin1(dv.unit)},
                                {QStringLiteral("as_of"), as_of}});
    }
    Tab& tb = tabs_[kCycle];
    tb.rows = rows;
    tb.columns = {QStringLiteral("indicator"), QStringLiteral("value"),  QStringLiteral("change_pct"),
                  QStringLiteral("change_bp"), QStringLiteral("unit"),   QStringLiteral("ticker"),
                  QStringLiteral("provider"),  QStringLiteral("as_of")};
    tb.title = tr("Business cycle indicators — %1").arg(as_of);
    SeriesMeta meta;
    meta.percent = 0;
    meta.source = tr("Yahoo Finance via yfinance (business_cycle.py)");
    meta.note = tr("Latest daily closes; changes are against the previous close");
    tb.meta = meta;
    tb.banner = warn;
}

// ── Equity risk premium ───────────────────────────────────────────────────────

bool MarketModelsPanel::read_erp_inputs(QJsonObject* params, QString* problem) const {
    for (int i = 0; i < kErpFieldCount; ++i) {
        const mmd::ErpField& f = mmd::kErpFields[i];
        const QString text = erp_edits_[i]->text().trimmed();
        if (text.isEmpty())
            continue;
        double v = 0.0;
        if (!ui::DataGridEditor::parse_number(text, &v)) {
            *problem = tr("%1: “%2” is not a number.").arg(tr(f.label), text);
            return false;
        }
        const bool ok = f.percent ? (v >= f.lo && v <= f.hi) : (v > f.lo && v <= f.hi);
        if (!ok) {
            *problem = f.percent ? tr("%1 must be between %2 and %3 (percent: 4.5 means 4.5 %).")
                                       .arg(tr(f.label), QString::number(f.lo), QString::number(f.hi))
                                 : tr("%1 must be above 0 and at most %2.").arg(tr(f.label), QString::number(f.hi));
            return false;
        }
        params->insert(QLatin1String(f.key), f.percent ? v / 100.0 : v);
    }
    const QString windows = erp_windows_->text().trimmed();
    if (!windows.isEmpty()) {
        static const QRegularExpression kSep(QStringLiteral("[,;\\s]+"));
        QJsonArray years;
        for (const QString& part : windows.split(kSep, Qt::SkipEmptyParts)) {
            bool ok = false;
            const int y = part.toInt(&ok);
            if (!ok || y < 1 || y > 30) {
                *problem = tr("WINDOWS takes whole years from 1 to 30, separated by commas (for example 5, 10, 20).");
                return false;
            }
            if (!years.contains(y))
                years.append(y);
        }
        if (!years.isEmpty())
            params->insert(QStringLiteral("windows_years"), years);
    }
    params->insert(QStringLiteral("timeout"), mmd::kNetTimeoutSec);
    return true;
}

void MarketModelsPanel::run_erp_from_form() {
    emit query_started();
    run_query(); // ESTIMATE = FETCH on this view (supersedes a run in flight)
}

void MarketModelsPanel::start_erp(bool force) {
    QJsonObject params;
    QString problem;
    if (!read_erp_inputs(&params, &problem)) {
        show_erp_inputs(problem, true);
        return;
    }
    erp_msg_->hide();
    const QString args = mmd::json_text(params);
    const QString id = QStringLiteral("mm_erp_") + mmd::args_tag(args);
    set_loading(kErp,
                tr("Estimating the equity risk premium — S&P 500 total return, 10Y Treasury, SPY P/E and "
                   "dividends…"),
                id);
    run_script(QString::fromLatin1(mmd::kErpScript), QStringLiteral("estimate_erp"), args, id, force);
}

void MarketModelsPanel::on_erp_result(const services::EconomicsResult& r) {
    Tab& t = tabs_[kErp];
    t.pending.clear();
    const QString err = error_of(r);
    if (!err.isEmpty()) {
        set_error(kErp, err);
        return;
    }
    erp_data_ = r.data;
    t.fetched_at = r.fetched_at;
    t.from_cache = r.from_cache;
    t.state = TabState::Ready;
    t.message.clear();
    ensure_page(kErp);
    populate_erp();
    LOG_DEBUG("MarketModelsPanel", "Equity risk premium as of " + mmd::str(erp_data_, "as_of"));
    if (current_view() == kErp)
        render_view();
}

QString MarketModelsPanel::erp_method_title(const QString& key) const {
    static const QRegularExpression kHist(QStringLiteral("^historical_([0-9]+)y$"));
    const QRegularExpressionMatch m = kHist.match(key);
    if (m.hasMatch())
        return tr("HISTORICAL · %1-YEAR REALISED").arg(m.captured(1));
    if (key == QLatin1String("implied_earnings_yield"))
        return tr("IMPLIED · EARNINGS YIELD");
    if (key == QLatin1String("supply_side"))
        return tr("SUPPLY SIDE · GRINOLD–KRONER");
    return mmd::humanize(key).toUpper();
}

QString MarketModelsPanel::erp_component_label(const QString& key) const {
    static const QHash<QString, const char*> kLabels = {
        {QStringLiteral("equity_return_annualized"),
         QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "S&P 500 total return (ann.)")},
        {QStringLiteral("treasury10y_return_annualized"),
         QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "10Y Treasury total return (ann.)")},
        {QStringLiteral("earnings_yield"), QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "Earnings yield")},
        {QStringLiteral("pe_ratio"), QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "P/E")},
        {QStringLiteral("real_risk_free_rate"), QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "Real 10Y rate")},
        {QStringLiteral("risk_free_rate_10y"), QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "10Y Treasury yield")},
        {QStringLiteral("dividend_yield"), QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "Dividend yield")},
        {QStringLiteral("buyback_yield"), QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "Buyback yield")},
        {QStringLiteral("real_earnings_growth"),
         QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "Real earnings growth")},
        {QStringLiteral("expected_inflation"),
         QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "Expected inflation")},
        {QStringLiteral("expected_equity_return"),
         QT_TRANSLATE_NOOP("fincept::screens::MarketModelsPanel", "Expected equity return")},
    };
    const auto it = kLabels.constFind(key);
    return it != kLabels.cend() ? tr(it.value()) : mmd::humanize(key);
}

QFrame* MarketModelsPanel::make_erp_card(const QString& key, const QJsonObject& e) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("mmCard"));
    auto* g = new QGridLayout(card);
    g->setContentsMargins(12, 8, 12, 10);
    g->setHorizontalSpacing(12);
    g->setVerticalSpacing(3);

    auto* title = mmd::make_label(card, "mmCardTitle", false);
    title->setText(erp_method_title(key));
    const double erp = mmd::num(e, "erp");
    auto* value = mmd::make_label(card, mmd::has(erp) ? "mmCardValue" : "mmCardValueDim", false);
    value->setText(mmd::frac_pct(erp));
    value->setAlignment(Qt::AlignRight | Qt::AlignTop);
    g->addWidget(title, 0, 0);
    g->addWidget(value, 0, 1, 3, 1, Qt::AlignRight | Qt::AlignTop);
    int row = 1;

    if (mmd::has(erp)) {
        static const char* const kOrder[] = {"equity_return_annualized", "treasury10y_return_annualized",
                                             "earnings_yield",           "pe_ratio",
                                             "real_risk_free_rate",      "dividend_yield",
                                             "buyback_yield",            "real_earnings_growth",
                                             "expected_inflation",       "expected_equity_return"};
        QStringList keys;
        for (const char* k : kOrder)
            if (e.contains(QLatin1String(k)))
                keys << QString::fromLatin1(k);
        for (auto it = e.begin(); it != e.end(); ++it) {
            const QString& k = it.key();
            if (!keys.contains(k) && k != QLatin1String("erp") && k != QLatin1String("method") &&
                k != QLatin1String("reason") && k != QLatin1String("window_start") && k != QLatin1String("window_end"))
                keys << k;
        }
        QStringList parts;
        for (const QString& k : std::as_const(keys)) {
            const QJsonValue v = e.value(k);
            QString text;
            if (v.isDouble())
                text = k == QLatin1String("pe_ratio") ? mmd::fmt::ratio(v.toDouble(), 2) : mmd::frac_pct(v.toDouble());
            else if (v.isString())
                text = v.toString();
            else
                continue;
            parts << tr("%1 %2").arg(erp_component_label(k), text);
        }
        if (!parts.isEmpty()) {
            auto* comp = mmd::make_label(card, "mmCardBody");
            comp->setText(parts.join(QStringLiteral("   ·   ")));
            g->addWidget(comp, row++, 0);
        }
        const QString from = mmd::str(e, "window_start");
        const QString to = mmd::str(e, "window_end");
        if (!from.isEmpty() || !to.isEmpty()) {
            auto* win = mmd::make_label(card, "mmNote");
            win->setText(tr("Window %1 → %2").arg(from, to));
            g->addWidget(win, row++, 0);
        }
        if (!mmd::str(e, "method").isEmpty()) {
            auto* desc = mmd::make_label(card, "mmNote");
            desc->setText(mmd::str(e, "method"));
            g->addWidget(desc, row++, 0);
        }
    } else {
        auto* why = mmd::make_label(card, "mmWarn");
        const QString reason = mmd::str(e, "reason");
        why->setText(tr("Not computed — %1").arg(reason.isEmpty() ? tr("no reason given") : reason));
        g->addWidget(why, row++, 0);
        if (key == QLatin1String("supply_side")) {
            // No free source exists for buybacks or long-run real growth: make the two inputs easy to find.
            auto* hint = mmd::make_label(card, "mmNote");
            hint->setText(tr("Supply-side ERP = dividend yield + buyback yield + real earnings growth + expected "
                             "inflation − risk-free rate. Enter BUYBACK YIELD and REAL EARNINGS GROWTH in the "
                             "inputs above (for example 2 and 1.8), then press ESTIMATE."));
            g->addWidget(hint, row++, 0);
            auto* go = new QPushButton(tr("ENTER BUYBACK && GROWTH"), card);
            go->setObjectName(QStringLiteral("econCsvBtn"));
            go->setCursor(Qt::PointingHandCursor);
            connect(go, &QPushButton::clicked, this, [this]() {
                if (erp_scroll_ && erp_scroll_->verticalScrollBar())
                    erp_scroll_->verticalScrollBar()->setValue(0);
                if (QLineEdit* e4 = erp_edits_[4]) {
                    e4->setFocus(Qt::OtherFocusReason);
                    e4->selectAll();
                }
            });
            g->addWidget(go, row++, 0, Qt::AlignLeft);
        }
    }
    g->setColumnStretch(0, 1);
    return card;
}

void MarketModelsPanel::populate_erp() {
    using ui::StatTile;
    const QJsonObject& d = erp_data_;
    const QJsonObject estimates = d.value(QStringLiteral("erp_estimates")).toObject();
    const QJsonObject inputs = d.value(QStringLiteral("inputs")).toObject();
    auto tile = [this](const char* key) { return erp_kpis_->tile(QString::fromLatin1(key)); };
    auto input_sub = [](const QJsonObject& in) {
        const QString as_of = mmd::str(in, "as_of");
        if (!as_of.isEmpty())
            return tr("as of %1").arg(as_of);
        return mmd::str(in, "source").startsWith(QLatin1String("input")) ? tr("your input") : QString();
    };

    // ── Headline tiles
    mmd::reset_tiles(erp_kpis_, mmd::kErpTiles);
    const QString cmethod = mmd::str(d, "consensus_method");
    if (StatTile* t = tile("consensus")) {
        t->set_value(mmd::frac_pct(mmd::num(d, "consensus_erp_estimate")));
        t->set_sub_label(tr("mean of the core estimates"));
        t->set_tooltip_text(cmethod);
    }
    if (StatTile* t = tile("range")) {
        const double lo = mmd::num(d, "erp_range_low");
        const double hi = mmd::num(d, "erp_range_high");
        if (mmd::has(lo) && mmd::has(hi))
            t->set_value(tr("%1 – %2").arg(mmd::frac_pct(lo), mmd::frac_pct(hi)));
        t->set_sub_label(tr("lowest to highest method"));
    }
    const QJsonObject rf_in = inputs.value(QStringLiteral("risk_free_rate_10y")).toObject();
    if (StatTile* t = tile("rf")) {
        t->set_value(mmd::frac_pct(mmd::num(rf_in, "value")));
        t->set_sub_label(input_sub(rf_in));
        t->set_tooltip_text(mmd::str(rf_in, "source"));
    }
    if (StatTile* t = tile("real")) {
        t->set_value(mmd::frac_pct(mmd::num(d, "real_risk_free_rate_10y")));
        t->set_sub_label(tr("(1 + rf) / (1 + inflation) − 1"));
    }
    const QJsonObject infl_in = inputs.value(QStringLiteral("expected_inflation")).toObject();
    const bool assumption = mmd::str(infl_in, "source").startsWith(QLatin1String("assumption"), Qt::CaseInsensitive);
    if (StatTile* t = tile("infl")) {
        t->set_value(mmd::frac_pct(mmd::num(infl_in, "value")));
        t->set_sub_label(assumption ? tr("ASSUMPTION — the Fed's 2 % goal") : input_sub(infl_in));
        if (assumption)
            t->set_tone(StatTile::Tone::Warning);
        t->set_tooltip_text(mmd::str(infl_in, "source"));
    }
    QStringList note;
    if (!cmethod.isEmpty())
        note << tr("Consensus = %1.").arg(cmethod);
    if (assumption)
        note << tr("Expected inflation is an assumption (the Federal Reserve's 2 % goal) because no FRED key is "
                   "set — set FRED_API_KEY for the 10-year breakeven, or enter EXPECTED INFLATION above.");
    erp_consensus_note_->setText(note.join(QLatin1Char('\n')));
    erp_consensus_note_->setVisible(!note.isEmpty());

    // ── One card per method
    while (QLayoutItem* item = erp_cards_->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    const QStringList methods = mmd::ordered_methods(estimates);
    for (const QString& k : methods)
        erp_cards_->addWidget(make_erp_card(k, estimates.value(k).toObject()));

    // ── Inputs: value, date and where each one came from
    static const char* const kInputOrder[] = {"risk_free_rate_10y", "expected_inflation", "pe_ratio",
                                              "dividend_yield",     "buyback_yield",      "real_earnings_growth"};
    QStringList in_keys;
    for (const char* k : kInputOrder)
        if (inputs.contains(QLatin1String(k)))
            in_keys << QString::fromLatin1(k);
    for (auto it = inputs.begin(); it != inputs.end(); ++it)
        if (!in_keys.contains(it.key()))
            in_keys << it.key();
    erp_inputs_table_->setRowCount(static_cast<int>(in_keys.size()));
    for (int r = 0; r < in_keys.size(); ++r) {
        const QString& k = in_keys.at(r);
        const QJsonObject in = inputs.value(k).toObject();
        const double v = mmd::num(in, "value");
        const QString value = !mmd::has(v)                     ? mmd::fmt::placeholder()
                              : k == QLatin1String("pe_ratio") ? mmd::fmt::ratio(v, 2)
                                                               : mmd::frac_pct(v);
        const QString as_of = mmd::str(in, "as_of");
        const QString source = mmd::str(in, "source");
        mmd::set_cell(erp_inputs_table_, r, 0, erp_component_label(k), false);
        mmd::set_cell(erp_inputs_table_, r, 1, value, true);
        mmd::set_cell(erp_inputs_table_, r, 2, as_of.isEmpty() ? mmd::fmt::placeholder() : as_of, false);
        mmd::set_cell(erp_inputs_table_, r, 3, source, false, source);
    }
    for (int c = 0; c < 3; ++c)
        erp_inputs_table_->resizeColumnToContents(c);
    fit_erp_inputs_table();

    const QJsonArray notes = d.value(QStringLiteral("notes")).toArray();
    QStringList note_text;
    for (const QJsonValue& n : notes)
        if (!n.toString().trimmed().isEmpty())
            note_text << n.toString().trimmed();
    erp_notes_->setText(note_text.isEmpty() ? QString() : tr("Notes: %1").arg(note_text.join(QStringLiteral("; "))));
    erp_notes_->setVisible(!note_text.isEmpty());

    // ── Rows for the title bar / CSV (values are decimal fractions per year, P/E a multiple)
    QJsonArray rows;
    for (const QString& k : methods) {
        const QJsonObject e = estimates.value(k).toObject();
        QStringList detail;
        for (auto it = e.begin(); it != e.end(); ++it) {
            if (it.key() == QLatin1String("erp") || it.key() == QLatin1String("method") ||
                it.key() == QLatin1String("reason"))
                continue;
            detail << it.key() + QLatin1Char('=') +
                          (it.value().isDouble() ? QString::number(it.value().toDouble(), 'g', 8) : it.value().toString());
        }
        rows.append(QJsonObject{{QStringLiteral("item"), k},
                                {QStringLiteral("kind"), QStringLiteral("estimate")},
                                {QStringLiteral("value"), mmd::json_num(mmd::num(e, "erp"))},
                                {QStringLiteral("detail"), detail.join(QStringLiteral("; "))},
                                {QStringLiteral("source"), mmd::str(e, "method").isEmpty() ? mmd::str(e, "reason")
                                                                                           : mmd::str(e, "method")},
                                {QStringLiteral("as_of"), mmd::str(e, "window_end")}});
    }
    for (const char* k : {"consensus_erp_estimate", "erp_range_low", "erp_range_high", "real_risk_free_rate_10y"})
        rows.append(QJsonObject{{QStringLiteral("item"), QString::fromLatin1(k)},
                                {QStringLiteral("kind"), QStringLiteral("summary")},
                                {QStringLiteral("value"), mmd::json_num(mmd::num(d, k))},
                                {QStringLiteral("detail"),
                                 QLatin1String(k) == QLatin1String("consensus_erp_estimate") ? cmethod : QString()},
                                {QStringLiteral("as_of"), mmd::str(d, "as_of")}});
    for (const QString& k : std::as_const(in_keys)) {
        const QJsonObject in = inputs.value(k).toObject();
        rows.append(QJsonObject{{QStringLiteral("item"), k},
                                {QStringLiteral("kind"), QStringLiteral("input")},
                                {QStringLiteral("value"), mmd::json_num(mmd::num(in, "value"))},
                                {QStringLiteral("source"), mmd::str(in, "source")},
                                {QStringLiteral("as_of"), mmd::str(in, "as_of")}});
    }
    Tab& tb = tabs_[kErp];
    tb.rows = rows;
    tb.columns = {QStringLiteral("item"),   QStringLiteral("kind"),   QStringLiteral("value"),
                  QStringLiteral("detail"), QStringLiteral("source"), QStringLiteral("as_of")};
    tb.title = tr("Equity risk premium — S&P 500 over the 10Y Treasury, %1").arg(mmd::str(d, "as_of"));
    SeriesMeta meta;
    meta.percent = 0;
    meta.units = mmd::str(d, "units");
    meta.source = tr("Yahoo Finance (^SP500TR, ^TNX, SPY); FRED T10YIE when FRED_API_KEY is set");
    meta.note = tr("Generated %1").arg(mmd::str(d, "generated_at"));
    tb.meta = meta;
    tb.banner.clear();
}

void MarketModelsPanel::fit_erp_inputs_table() {
    if (!erp_inputs_table_)
        return;
    erp_inputs_table_->resizeRowsToContents();
    int h = erp_inputs_table_->horizontalHeader()->sizeHint().height();
    for (int r = 0; r < erp_inputs_table_->rowCount(); ++r)
        h += erp_inputs_table_->rowHeight(r);
    h += 2 * erp_inputs_table_->frameWidth() + 2;
    if (erp_inputs_table_->height() != h)
        erp_inputs_table_->setFixedHeight(h);
}

void MarketModelsPanel::show_erp_inputs(const QString& note, bool error) {
    QString text = note;
    if (error && !erp_data_.isEmpty())
        text += QLatin1Char('\n') + tr("The estimate below is from the previous run.");
    erp_msg_->setText(text);
    mmd::set_role(erp_msg_, error ? "mmError" : "mmNote");
    erp_msg_->setVisible(!text.isEmpty());
    if (current_view() != kErp || tabs_[kErp].page < 0)
        return;
    publish_rows(kErp);
    show_content_page(tabs_[kErp].page);
    if (erp_scroll_ && erp_scroll_->verticalScrollBar())
        erp_scroll_->verticalScrollBar()->setValue(0);
}

// ── Yield curve ───────────────────────────────────────────────────────────────

void MarketModelsPanel::load_treasury(bool force) {
    curve_msg_->hide();
    set_loading(kCurve, tr("Loading the latest US Treasury par yield curve (Federal Reserve H.15)…"),
                QString::fromLatin1(mmd::kUstRequest));
    run_script(QString::fromLatin1(mmd::kUstScript), QStringLiteral("yield_curve"), QString(),
               QString::fromLatin1(mmd::kUstRequest), force);
}

void MarketModelsPanel::on_treasury_result(const services::EconomicsResult& r) {
    Tab& t = tabs_[kCurve];
    t.pending.clear();
    QString err = error_of(r);
    QJsonArray points; // [{maturity: years, yield: percent}]
    QString date;
    if (err.isEmpty()) {
        const QJsonArray rows = r.data.value(QStringLiteral("data")).toArray();
        for (const QJsonValue& v : rows) // the latest curve in the answer
            date = std::max(date, mmd::str(v.toObject(), "date"));
        QVector<QPair<double, double>> pts;
        for (const QJsonValue& v : rows) {
            const QJsonObject o = v.toObject();
            if (mmd::str(o, "date") != date)
                continue;
            const double years = mmd::h15_years(mmd::str(o, "maturity"));
            const double rate = mmd::num(o, "rate"); // decimal
            if (mmd::has(years) && mmd::has(rate))
                pts.append({years, std::round(rate * 100.0 * 1e6) / 1e6}); // to percent, without float dust
        }
        std::sort(pts.begin(), pts.end());
        for (const auto& p : std::as_const(pts))
            points.append(QJsonObject{{QStringLiteral("maturity"), p.first}, {QStringLiteral("yield"), p.second}});
        if (points.size() < 2)
            err = tr("the Federal Reserve answer held no usable curve points");
    }
    if (!err.isEmpty()) {
        set_error(kCurve, tr("Could not load the US Treasury curve: %1").arg(err));
        return;
    }
    curve_fetched_at_ = r.fetched_at;
    curve_from_cache_ = r.from_cache;
    fill_curve_grid(points);
    curve_source_ = QString::fromLatin1(mmd::kSrcTreasury);
    curve_as_of_ = date;
    const int semi = curve_freq_combo_->findData(2);
    if (semi >= 0)
        curve_freq_combo_->setCurrentIndex(semi); // Treasury notes and bonds pay semiannual coupons
    update_curve_source_label();
    LOG_DEBUG("MarketModelsPanel", QString("US Treasury curve %1: %2 points").arg(date).arg(points.size()));
    run_curve();
}

void MarketModelsPanel::fill_curve_grid(const QJsonArray& rows) {
    filling_grid_ = true;
    curve_grid_->from_json(rows);
    filling_grid_ = false;
}

void MarketModelsPanel::update_curve_source_label() {
    if (!curve_src_lbl_)
        return;
    if (curve_source_ == QLatin1String(mmd::kSrcTreasury))
        curve_src_lbl_->setText(tr("US Treasury par yields · Federal Reserve H.15 · %1").arg(curve_as_of_));
    else if (curve_source_ == QLatin1String(mmd::kSrcManual))
        curve_src_lbl_->setText(tr("Custom curve — press RUN to analyse it"));
    else
        curve_src_lbl_->setText(tr("No curve loaded — type or paste points, or LOAD US TREASURY"));
}

void MarketModelsPanel::run_curve_from_editor() {
    emit query_started();
    run_query(); // RUN analyses the curve in the editor (run_view() → run_curve())
}

void MarketModelsPanel::run_curve() {
    QVector<QPair<double, double>> pts; // years, percent
    const QJsonArray rows = curve_grid_->to_json();
    for (const QJsonValue& v : rows) {
        const QJsonObject o = v.toObject();
        const double m = mmd::num(o, "maturity");
        const double y = mmd::num(o, "yield");
        if (mmd::has(m) && mmd::has(y))
            pts.append({m, y});
    }
    std::sort(pts.begin(), pts.end());

    QString problem;
    if (pts.size() < 2)
        problem = tr("Enter at least two curve points — maturity in years (3M or 10Y also work when pasting) and "
                     "yield in percent.");
    for (int i = 0; i < pts.size() && problem.isEmpty(); ++i) {
        const double m = pts.at(i).first;
        const double y = pts.at(i).second;
        if (m <= 0.0 || m > 100.0)
            problem = tr("Maturity %1 is out of range — enter years between 0 and 100 (0.25 = 3 months).")
                          .arg(QString::number(m, 'g', 6));
        else if (std::abs(y) > 100.0)
            problem = tr("Yield %1 is out of range — enter yields in percent (4.25 means 4.25 %).")
                          .arg(QString::number(y, 'g', 6));
        else if (i > 0 && std::abs(m - pts.at(i - 1).first) < 1e-9)
            problem = tr("Maturity %1 appears twice.").arg(mmd::tenor_label(m));
    }
    if (!problem.isEmpty()) {
        if (tabs_[kCurve].state == TabState::Loading) { // came from a Treasury load: do not stay "loading"
            tabs_[kCurve].state = TabState::Error;
            tabs_[kCurve].message = problem;
            tabs_[kCurve].pending.clear();
        }
        show_curve_editor(problem, true);
        return;
    }

    // A new run: the results on display stay until it produces something.
    CurveRun run;
    QStringList notes; // soft notes shown under the editor after the run
    double max_abs = 0.0;
    for (const auto& p : std::as_const(pts))
        max_abs = std::max(max_abs, std::abs(p.second));
    if (max_abs > 0.0 && max_abs < 0.25)
        notes << tr("Every yield is below 0.25 % — the YIELD column is in percent (4.25 means 4.25 %), not a "
                    "decimal.");
    if (curve_grid_->invalid_row_count() > 0)
        notes << tr("%n row(s) with problems were left out (see the line under the table).", "",
                    curve_grid_->invalid_row_count());
    run.note = notes.join(QLatin1Char('\n'));

    QJsonArray jm;
    QJsonArray jy;
    for (const auto& p : std::as_const(pts)) {
        run.m.append(p.first);
        run.y.append(p.second / 100.0); // the analytics take decimals
        jm.append(p.first);
        jy.append(p.second / 100.0);
    }
    run.freq = curve_freq_combo_->currentData().toInt();
    run.source = curve_source_;
    run.as_of = curve_as_of_;
    const bool treasury = run.source == QLatin1String(mmd::kSrcTreasury);
    run.fetched_at = treasury ? curve_fetched_at_ : QDateTime();
    run.from_cache = treasury && curve_from_cache_;

    const QJsonObject curve{{QStringLiteral("maturities"), jm}, {QStringLiteral("yields"), jy}};
    QJsonObject boot = curve;
    boot.insert(QStringLiteral("frequency"), run.freq);
    QVector<QPair<QString, QString>> requests = {{QStringLiteral("curve_shape"), mmd::json_text(curve)},
                                                 {QStringLiteral("bootstrap"), mmd::json_text(boot)}};
    fi_pending_.clear();
    if (pts.size() >= 4)
        requests.append(qMakePair(QStringLiteral("nelson_siegel"), mmd::json_text(curve)));
    else
        run.errors.insert(QStringLiteral("nelson_siegel"), tr("needs at least 4 curve points"));
    pending_run_ = run;
    // Register every request before starting any: a cached answer arrives synchronously, inside execute().
    for (const auto& rq : std::as_const(requests))
        fi_pending_.insert(rq.first, QStringLiteral("mm_fi_") + rq.first + QLatin1Char('_') + mmd::args_tag(rq.second));
    curve_run_active_ = true;
    curve_msg_->hide();
    set_loading(kCurve, tr("Running curve analytics — shape, bootstrap, forwards, Nelson–Siegel…"),
                QString::fromLatin1(mmd::kFiRunMarker));
    for (const auto& rq : std::as_const(requests)) {
        const QString id = fi_pending_.value(rq.first);
        if (!id.isEmpty())
            run_script(QString::fromLatin1(mmd::kFiScript), rq.first, rq.second, id, false);
    }
}

void MarketModelsPanel::start_forward_curve(const QJsonArray& spot_curve) {
    QJsonArray rates;
    for (const QJsonValue& v : spot_curve) {
        const QJsonObject o = v.toObject();
        const double m = mmd::num(o, "maturity");
        const double s = mmd::num(o, "spot_rate");
        if (mmd::has(m) && mmd::has(s))
            rates.append(QJsonObject{{QStringLiteral("maturity"), m}, {QStringLiteral("spot_rate"), s}});
    }
    if (rates.size() < 2) {
        pending_run_.errors.insert(QStringLiteral("forward_curve"), tr("needs at least two zero rates"));
        return;
    }
    const QString args = mmd::json_text(QJsonObject{{QStringLiteral("spot_rates"), rates}});
    const QString id = QStringLiteral("mm_fi_forward_curve_") + mmd::args_tag(args);
    fi_pending_.insert(QStringLiteral("forward_curve"), id);
    run_script(QString::fromLatin1(mmd::kFiScript), QStringLiteral("forward_curve"), args, id, false);
}

void MarketModelsPanel::on_fi_result(const QString& command, const services::EconomicsResult& r) {
    fi_pending_.remove(command);
    const QString err = error_of(r);
    if (err.isEmpty())
        pending_run_.data.insert(command, r.data);
    else
        pending_run_.errors.insert(command, err);
    if (command == QLatin1String("bootstrap") && err.isEmpty())
        start_forward_curve(r.data.value(QStringLiteral("spot_curve")).toArray()); // may finish the run itself
    if (fi_pending_.isEmpty() && curve_run_active_)
        finish_curve_run();
}

void MarketModelsPanel::finish_curve_run() {
    curve_run_active_ = false;
    Tab& t = tabs_[kCurve];
    const CurveRun& run = pending_run_;
    if (!run.data.contains(QStringLiteral("curve_shape")) && !run.data.contains(QStringLiteral("bootstrap"))) {
        // Nothing usable — the failures nearly always share one cause; show the first. The previous results
        // (if any) stay on the page.
        QString msg = run.errors.value(QStringLiteral("curve_shape"));
        if (msg.isEmpty())
            msg = run.errors.value(QStringLiteral("bootstrap"));
        set_error(kCurve, tr("Curve analytics failed: %1").arg(msg.isEmpty() ? tr("no result") : msg));
        return;
    }
    shown_run_ = pending_run_;
    t.state = TabState::Ready;
    t.message.clear();
    t.pending.clear();
    t.fetched_at = shown_run_.fetched_at;
    t.from_cache = shown_run_.from_cache;
    populate_curve();
    curve_msg_->setText(shown_run_.note);
    mmd::set_role(curve_msg_, "mmWarn");
    curve_msg_->setVisible(!shown_run_.note.isEmpty());
    if (current_view() == kCurve)
        render_view();
}

void MarketModelsPanel::clear_curve_results(const QString& message) {
    mmd::reset_tiles(curve_kpis_, mmd::kCurveTiles);
    curve_caption_->setText(message);
    curve_chart_->clear();
    curve_chart_->set_title({});
    curve_chart_->set_state(ui::TimeSeriesChart::State::Empty, message);
    curve_table_->setRowCount(0);
}

void MarketModelsPanel::populate_curve() {
    using ui::StatTile;
    const CurveRun& run = shown_run_;
    const QJsonObject shape = run.data.value(QStringLiteral("curve_shape"));
    const QJsonObject boot = run.data.value(QStringLiteral("bootstrap"));
    const QJsonObject fwd = run.data.value(QStringLiteral("forward_curve"));
    const QJsonObject ns = run.data.value(QStringLiteral("nelson_siegel"));
    const QJsonArray spot = boot.value(QStringLiteral("spot_curve")).toArray();
    const QJsonArray fwds = fwd.value(QStringLiteral("forward_curve")).toArray();
    const QJsonArray fitted = ns.value(QStringLiteral("fitted_curve")).toArray();
    const QJsonArray fitted_in = ns.value(QStringLiteral("fitted_at_inputs")).toArray();
    const bool treasury = run.source == QLatin1String(mmd::kSrcTreasury);
    auto tile = [this](const char* key) { return curve_kpis_->tile(QString::fromLatin1(key)); };

    // ── Tiles
    mmd::reset_tiles(curve_kpis_, mmd::kCurveTiles);
    if (!shape.isEmpty()) {
        const QString shp = mmd::str(shape, "curve_shape");
        const QString regime = mmd::str(shape, "slope_regime");
        const double sm = mmd::num(shape, "short_maturity");
        const double mm = mmd::num(shape, "mid_maturity");
        const double lm = mmd::num(shape, "long_maturity");
        if (StatTile* t = tile("shape")) {
            t->set_value(shp.isEmpty() ? mmd::fmt::placeholder() : shp);
            if (!regime.isEmpty() && regime != shp)
                t->set_sub_label(tr("slope: %1").arg(regime));
            if (shp.contains(QLatin1String("Invert"), Qt::CaseInsensitive))
                t->set_tone(StatTile::Tone::Warning);
            t->set_tooltip_text(regime.isEmpty() ? shp : tr("%1\nSlope regime: %2").arg(shp, regime));
        }
        if (StatTile* t = tile("slope")) {
            t->set_value(mmd::bps(mmd::num(shape, "slope_bps")));
            t->set_sub_label(tr("%1 → %2").arg(mmd::tenor_label(sm), mmd::tenor_label(lm)));
            t->set_tooltip_text(tr("Longest yield %1 minus shortest yield %2")
                                    .arg(mmd::frac_pct(mmd::num(shape, "long_rate"), 3),
                                         mmd::frac_pct(mmd::num(shape, "short_rate"), 3)));
        }
        auto spread = [&](const char* key, const char* field, const QString& what, const QString& needs) {
            StatTile* t = tile(key);
            if (!t)
                return;
            const double v = mmd::num(shape, field);
            t->set_value(mmd::bps(v));
            t->set_sub_label(mmd::has(v) ? what : needs);
            if (mmd::has(v) && v < 0.0)
                t->set_tone(StatTile::Tone::Warning); // inverted segment
            t->set_tooltip_text(what);
        };
        spread("s2s10s", "spread_2s10s", tr("10Y − 2Y"), tr("needs 2Y and 10Y within the curve"));
        spread("s3m10y", "spread_3m10y", tr("10Y − 3M"), tr("needs 3M and 10Y within the curve"));
        if (StatTile* t = tile("fly")) {
            t->set_value(mmd::bps(mmd::num(shape, "butterfly_bps")));
            t->set_sub_label(mmd::has(mm) ? tr("2 × %1 − %2 − %3")
                                                .arg(mmd::tenor_label(mm), mmd::tenor_label(sm), mmd::tenor_label(lm))
                                          : tr("needs at least 3 points"));
            t->set_tooltip_text(tr("Curvature: twice the belly yield minus the short and long yields"));
        }
        if (StatTile* t = tile("signal")) {
            const QString signal = mmd::str(shape, "market_signal");
            t->set_value(signal.isEmpty() ? mmd::fmt::placeholder() : signal);
            t->set_tooltip_text(signal);
        }
    } else {
        const QString why = run.errors.value(QStringLiteral("curve_shape"));
        for (const mmd::TileDef& def : mmd::kCurveTiles)
            if (StatTile* t = tile(def.key))
                t->set_state(StatTile::State::Error, why);
    }

    // ── Notes: signal, interpolation notes, the Nelson–Siegel fit, partial failures, conventions
    QStringList lines;
    if (!shape.isEmpty() && !mmd::str(shape, "market_signal").isEmpty())
        lines << tr("Signal: %1").arg(mmd::str(shape, "market_signal"));
    QStringList shape_notes;
    for (const QJsonValue& n : shape.value(QStringLiteral("notes")).toArray())
        shape_notes << n.toString();
    if (!shape_notes.isEmpty())
        lines << tr("Shape: %1").arg(shape_notes.join(QStringLiteral("; ")));
    if (!ns.isEmpty()) {
        const QJsonObject p = ns.value(QStringLiteral("parameters")).toObject();
        const QJsonObject fs = ns.value(QStringLiteral("fit_statistics")).toObject();
        const double rmse = mmd::num(fs, "rmse");
        lines << tr("Nelson–Siegel: β0 %1 · β1 %2 · β2 %3 · τ %4 · RMSE %5 · R² %6 · largest miss %7")
                     .arg(mmd::frac_pct(mmd::num(p, "beta0"), 3), mmd::frac_pct(mmd::num(p, "beta1"), 3),
                          mmd::frac_pct(mmd::num(p, "beta2"), 3), mmd::plain(mmd::num(p, "tau"), 3),
                          mmd::bps(mmd::has(rmse) ? rmse * 10000.0 : rmse, 1), mmd::plain(mmd::num(fs, "r_squared"), 3),
                          mmd::bps(mmd::num(fs, "max_abs_error_bps"), 1));
    } else if (run.errors.contains(QStringLiteral("nelson_siegel"))) {
        lines << tr("Nelson–Siegel: %1").arg(run.errors.value(QStringLiteral("nelson_siegel")));
    }
    if (run.errors.contains(QStringLiteral("bootstrap")))
        lines << tr("Bootstrap failed: %1").arg(run.errors.value(QStringLiteral("bootstrap")));
    if (run.errors.contains(QStringLiteral("forward_curve")))
        lines << tr("Forward curve: %1").arg(run.errors.value(QStringLiteral("forward_curve")));
    if (run.errors.contains(QStringLiteral("curve_shape")))
        lines << tr("Shape analysis failed: %1").arg(run.errors.value(QStringLiteral("curve_shape")));
    lines << (run.freq > 0
                  ? tr("Par yields are bond-equivalent with %n coupon(s) a year; zero and forward rates are annually "
                       "compounded.",
                       "", run.freq)
                  : tr("Inputs are zero-coupon rates; zero and forward rates are annually compounded."));
    curve_caption_->setText(lines.join(QLatin1Char('\n')));

    // ── Chart: par input, zero, forward (steps between maturities), Nelson–Siegel fit
    curve_chart_->clear();
    int plotted = 0;
    auto plot = [&](const QString& name, const QVector<QPointF>& pts, int palette, ui::TimeSeriesChart::Style style,
                    bool dashed, qreal width) {
        if (pts.size() < 2)
            return;
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = name;
        s.id = name;
        s.points = pts;
        s.style = style;
        s.color = ui::TimeSeriesChart::ColorRole::Palette;
        s.palette_index = palette;
        s.dashed = dashed;
        s.line_width = width;
        s.rebase = false;
        s.formatter = [](double v) { return mmd::fmt::pct(v, 3, false); };
        curve_chart_->add_series(s);
        ++plotted;
    };
    QVector<QPointF> par_pts;
    for (int i = 0; i < run.m.size(); ++i)
        par_pts.append(QPointF(mmd::axis_x(run.m.at(i)), run.y.at(i) * 100.0));
    QVector<QPointF> zero_pts;
    for (const QJsonValue& v : spot) {
        const QJsonObject o = v.toObject();
        const double m = mmd::num(o, "maturity");
        const double s = mmd::num(o, "spot_rate");
        if (mmd::has(m) && mmd::has(s))
            zero_pts.append(QPointF(mmd::axis_x(m), s * 100.0));
    }
    QVector<QPointF> fwd_pts;
    double last_end = mmd::no_value();
    double last_fwd = mmd::no_value();
    for (const QJsonValue& v : fwds) {
        const QJsonObject o = v.toObject();
        const double s = mmd::num(o, "start");
        const double e = mmd::num(o, "end");
        const double f = mmd::num(o, "forward_rate");
        if (!mmd::has(s) || !mmd::has(e) || !mmd::has(f))
            continue;
        fwd_pts.append(QPointF(mmd::axis_x(s), f * 100.0));
        last_end = e;
        last_fwd = f;
    }
    if (!fwd_pts.isEmpty() && mmd::has(last_end))
        fwd_pts.append(QPointF(mmd::axis_x(last_end), last_fwd * 100.0)); // close the last step
    QVector<QPointF> ns_pts;
    for (const QJsonValue& v : fitted) {
        const QJsonObject o = v.toObject();
        const double m = mmd::num(o, "maturity");
        const double y = mmd::num(o, "yield");
        if (mmd::has(m) && mmd::has(y))
            ns_pts.append(QPointF(mmd::axis_x(m), y * 100.0));
    }
    using Style = ui::TimeSeriesChart::Style;
    plot(tr("Par yield (input)"), par_pts, 0, Style::Line, false, 2.0);
    plot(tr("Zero rate"), zero_pts, 1, Style::Line, false, 1.5);
    plot(tr("Forward rate"), fwd_pts, 2, Style::Step, false, 1.5);
    plot(tr("Nelson–Siegel fit"), ns_pts, 3, Style::Line, true, 1.5);
    curve_chart_->set_title(treasury ? tr("US TREASURY PAR CURVE · %1").arg(run.as_of) : tr("CUSTOM CURVE"));
    curve_chart_->set_export_name(treasury ? QStringLiteral("us_treasury_curve_") + run.as_of
                                           : QStringLiteral("custom_curve"));
    if (plotted > 0)
        curve_chart_->set_state(ui::TimeSeriesChart::State::Ready);
    else
        curve_chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("Nothing to plot"));

    // ── Table + rows for the title bar / CSV
    const QStringList headers = {tr("MATURITY"),        tr("PAR YIELD"),         tr("ZERO (BOND BASIS)"),
                                 tr("ZERO (ANNUAL)"),   tr("DISCOUNT FACTOR"),   tr("FORWARD (PREV → T)"),
                                 tr("NELSON–SIEGEL"),   tr("FIT ERROR")};
    curve_table_->clear();
    curve_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    curve_table_->setColumnCount(static_cast<int>(headers.size()));
    curve_table_->setHorizontalHeaderLabels(headers);
    if (QTableWidgetItem* h = curve_table_->horizontalHeaderItem(2))
        h->setToolTip(run.freq > 0 ? tr("Zero rate compounded %n time(s) a year, like the par yields", "", run.freq)
                                    : tr("Zero rate"));
    if (QTableWidgetItem* h = curve_table_->horizontalHeaderItem(5))
        h->setToolTip(tr("Forward rate from the previous maturity to this one (annual compounding)"));
    curve_table_->setRowCount(static_cast<int>(run.m.size()));
    QJsonArray rows;
    for (int i = 0; i < run.m.size(); ++i) {
        const double m = run.m.at(i);
        const double par = run.y.at(i);
        const QJsonObject sp = mmd::row_at(spot, "maturity", m);
        const QJsonObject fw = mmd::row_at(fwds, "end", m);
        const QJsonObject fit = mmd::row_at(fitted_in, "maturity", m);
        const double z_per = mmd::num(sp, "spot_rate_periodic");
        const double z_ann = mmd::num(sp, "spot_rate");
        const double df = mmd::num(sp, "discount_factor");
        const double f = mmd::num(fw, "forward_rate");
        const double nsv = mmd::num(fit, "fitted");
        const double resid = mmd::num(fit, "residual_bps");
        mmd::set_cell(curve_table_, i, 0, mmd::tenor_label(m), false,
                      tr("%1 years").arg(QString::number(m, 'g', 6)));
        mmd::set_cell(curve_table_, i, 1, mmd::frac_pct(par, 3), true);
        mmd::set_cell(curve_table_, i, 2, mmd::frac_pct(z_per, 3), true);
        mmd::set_cell(curve_table_, i, 3, mmd::frac_pct(z_ann, 3), true);
        mmd::set_cell(curve_table_, i, 4, mmd::plain(df, 6), true);
        mmd::set_cell(curve_table_, i, 5, mmd::frac_pct(f, 3), true,
                      fw.isEmpty() ? QString() : mmd::str(fw, "period"));
        mmd::set_cell(curve_table_, i, 6, mmd::frac_pct(nsv, 3), true);
        mmd::set_cell(curve_table_, i, 7, mmd::bps(resid, 1), true);
        rows.append(QJsonObject{{QStringLiteral("maturity_years"), m},
                                {QStringLiteral("tenor"), mmd::tenor_label(m)},
                                {QStringLiteral("par_yield_pct"), mmd::json_num(par * 100.0)},
                                {QStringLiteral("zero_bond_basis_pct"), mmd::json_num(z_per * 100.0)},
                                {QStringLiteral("zero_annual_pct"), mmd::json_num(z_ann * 100.0)},
                                {QStringLiteral("discount_factor"), mmd::json_num(df)},
                                {QStringLiteral("forward_from_prev_pct"), mmd::json_num(f * 100.0)},
                                {QStringLiteral("nelson_siegel_pct"), mmd::json_num(nsv * 100.0)},
                                {QStringLiteral("fit_error_bp"), mmd::json_num(resid)}});
    }
    curve_table_->resizeColumnsToContents(); // once, after the fill
    curve_table_->horizontalHeader()->setStretchLastSection(true);
    curve_has_results_ = true;

    Tab& tb = tabs_[kCurve];
    tb.rows = rows;
    tb.columns = {QStringLiteral("maturity_years"),      QStringLiteral("tenor"),
                  QStringLiteral("par_yield_pct"),       QStringLiteral("zero_bond_basis_pct"),
                  QStringLiteral("zero_annual_pct"),     QStringLiteral("discount_factor"),
                  QStringLiteral("forward_from_prev_pct"), QStringLiteral("nelson_siegel_pct"),
                  QStringLiteral("fit_error_bp")};
    tb.title = treasury ? tr("Yield curve analytics — US Treasury par curve, %1").arg(run.as_of)
                        : tr("Yield curve analytics — custom curve, %n point(s)", "", static_cast<int>(run.m.size()));
    SeriesMeta meta;
    meta.percent = 0;
    meta.units = tr("Percent; zero and forward rates annually compounded");
    meta.source = treasury ? tr("Federal Reserve H.15 Treasury constant-maturity (par) yields; analytics: "
                                "fixedIncome/cli.py")
                           : tr("Curve entered by the user; analytics: fixedIncome/cli.py");
    meta.note = run.note;
    tb.meta = meta;
    tb.banner.clear();
}

void MarketModelsPanel::show_curve_editor(const QString& note, bool error) {
    QString text = note;
    if (error && curve_has_results_)
        text += QLatin1Char('\n') + tr("The results shown are from the previous run.");
    curve_msg_->setText(text);
    mmd::set_role(curve_msg_, error ? "mmError" : "mmNote");
    curve_msg_->setVisible(!text.isEmpty());
    if (!curve_has_results_)
        clear_curve_results(tr("Enter or paste a curve on the right — maturity in years, yield in percent — then "
                               "press RUN, or LOAD US TREASURY."));
    if (current_view() != kCurve || tabs_[kCurve].page < 0)
        return;
    publish_rows(kCurve);
    show_content_page(tabs_[kCurve].page);
    if (curve_grid_)
        curve_grid_->setFocus(Qt::OtherFocusReason);
}

// ── Persistence ───────────────────────────────────────────────────────────────

QVariantMap MarketModelsPanel::save_panel_state() const {
    QVariantMap state = EconPanelBase::save_panel_state();
    // Pages not built this session keep the inputs they were restored with.
    QVariantMap mm = saved_inputs_;
    if (erp_root_) {
        QVariantMap erp;
        for (int i = 0; i < kErpFieldCount; ++i)
            erp.insert(QString::fromLatin1(mmd::kErpFields[i].key), erp_edits_[i]->text());
        erp.insert(QStringLiteral("windows_years"), erp_windows_->text());
        mm.insert(QStringLiteral("erp"), erp);
    }
    if (curve_root_) {
        mm.insert(QStringLiteral("curve_freq"), curve_freq_combo_->currentData().toInt());
        // A Treasury curve is fetched again; a curve the user entered is kept as typed (invalid rows included).
        if (curve_source_ == QLatin1String(mmd::kSrcManual))
            mm.insert(QStringLiteral("curve_manual"), curve_grid_->raw_json().toVariantList());
        else
            mm.remove(QStringLiteral("curve_manual"));
    }
    if (!mm.isEmpty())
        state.insert(QString::fromLatin1(mmd::kStateKey), mm);
    return state;
}

void MarketModelsPanel::apply_saved_inputs(int view) {
    if (saved_inputs_.isEmpty())
        return;
    if (view == kErp && erp_root_) {
        const QVariantMap erp = saved_inputs_.value(QStringLiteral("erp")).toMap();
        for (int i = 0; i < kErpFieldCount; ++i)
            erp_edits_[i]->setText(erp.value(QString::fromLatin1(mmd::kErpFields[i].key)).toString());
        erp_windows_->setText(erp.value(QStringLiteral("windows_years")).toString());
    } else if (view == kCurve && curve_root_) {
        const int freq = curve_freq_combo_->findData(saved_inputs_.value(QStringLiteral("curve_freq"), 2).toInt());
        if (freq >= 0)
            curve_freq_combo_->setCurrentIndex(freq);
        const QJsonArray manual =
            QJsonArray::fromVariantList(saved_inputs_.value(QStringLiteral("curve_manual")).toList());
        if (!manual.isEmpty() && tabs_[kCurve].state != TabState::Loading) {
            fill_curve_grid(manual);
            curve_source_ = QString::fromLatin1(mmd::kSrcManual);
            curve_as_of_.clear();
            update_curve_source_label();
        }
    }
}

void MarketModelsPanel::restore_panel_state(const QVariantMap& state) {
    restoring_ = true; // the VIEW combo may change: point the pane at the view once, at the end
    EconPanelBase::restore_panel_state(state);
    saved_inputs_ = state.value(QString::fromLatin1(mmd::kStateKey)).toMap();
    for (int v = 0; v < kViewCount; ++v)
        apply_saved_inputs(v); // pages already built; the others get them when they are built
    restoring_ = false;
    sync_load_view();
}

} // namespace fincept::screens
