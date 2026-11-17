// src/ui/widgets/ResultLayout.cpp
//
// Shape classification behind ResultView — see the header. Kept free of
// widgets so the same rules run in tests against recorded payloads.

#include "ui/widgets/ResultLayout.h"

#include "ui/formatting/NumberFormat.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QMap>
#include <QRegularExpression>
#include <QTime>
#include <QTimeZone>

#include <algorithm>
#include <climits>
#include <cmath>
#include <optional>

namespace fincept::ui::result {

namespace {

namespace fmt = fincept::ui::formatting;

constexpr double kRlNaN = std::numeric_limits<double>::quiet_NaN();
constexpr int kRlMaxLineSeries = 8;
constexpr int kRlMaxBarSeries = 6;
constexpr int kRlMaxHeatmapEdge = 60;
constexpr double kRlLineMagnitudeRatio = 50.0;
constexpr double kRlBarMagnitudeRatio = 10.0;

QString rl_tr(const char* text) {
    return QCoreApplication::translate("ResultView", text);
}

// ── Key words ────────────────────────────────────────────────────────────────

QStringList rl_tokens(const QString& key) {
    static const QRegularExpression kSplit(QStringLiteral("[^a-z0-9]+"));
    return key.toLower().split(kSplit, Qt::SkipEmptyParts);
}

bool rl_any(const QStringList& tokens, const QSet<QString>& words) {
    for (const QString& t : tokens)
        if (words.contains(t))
            return true;
    return false;
}

const QSet<QString>& rl_rate_words() {
    static const QSet<QString> s = {
        "rate",       "rates",       "yield",     "yields",    "ytm",       "vol",       "vols",
        "volatility", "volatilities", "sigma",    "irr",       "return",    "returns",   "cagr",
        "probability", "probabilities", "prob",   "pd",        "lgd",       "weight",    "weights",
        "margin",     "margins",     "growth",    "drawdown",  "spread",    "spreads",   "coupon",
        "wacc",       "roe",         "roa",       "roic",      "roce",      "tax",       "inflation",
        "payout",     "utilization", "allocation", "ownership", "retention", "dilution", "discount",
        "hurdle",     "var",         "cvar",      "es",        "shortfall", "recovery",  "hazard",
        "confidence", "tracking",    "default",   "survival",  "volatilty"};
    return s;
}

// Words that make a rate-looking key a plain factor / ratio instead.
const QSet<QString>& rl_not_rate_words() {
    static const QSet<QString> s = {"factor", "factors", "multiplier", "ratio",   "turnover", "score",
                                    "count",  "days",    "years",      "coverage", "index",   "periods",
                                    "steps",  "time",    "multiple",   "duration", "half",    "level"};
    return s;
}

const QSet<QString>& rl_money_words() {
    static const QSet<QString> s = {
        "price",     "prices",     "npv",      "pv",          "fv",        "cost",        "costs",
        "notional",  "principal",  "amount",   "amounts",     "cash",      "cashflow",    "cashflows",
        "revenue",   "revenues",   "sales",    "ebitda",      "ebit",      "income",      "debt",
        "payment",   "payments",   "balance",  "proceeds",    "premium",   "premiums",    "capital",
        "assets",    "liabilities", "nav",     "aum",         "wealth",    "profit",      "pnl",
        "gain",      "gains",      "loss",     "losses",      "exposure",  "ead",         "equity",
        "valuation", "ev",         "tv",       "fcf",         "fcff",      "fcfe",        "nopat",
        "capex",     "dividend",   "dividends", "interest",   "investment", "invested",   "consideration",
        "worth",     "valuations", "mv",       "budget",      "savings",   "collateral",
        "cva",       "dva",        "fva",      "xva",         "pfe",       "epe",         "ene",
        "mtm",       "arr",        "gmv",      "deposits",    "loans",     "payout",      "synergy",
        "synergies", "fees",       "fee"};
    return s;
}

// "<x>_value" is an amount only for these x ("present_value", "equity_value");
// a bare "value" or "function_value" is just a number.
bool rl_is_money_key(const QStringList& t) {
    static const QSet<QString> kValueOf = {"present", "fair",      "market",   "equity",   "enterprise", "terminal",
                                           "book",    "face",      "portfolio", "notional", "future",    "deal",
                                           "offer",   "purchase",  "residual", "firm",     "liquidation", "salvage",
                                           "debt",    "asset",     "exit",     "levered",  "unlevered",  "intrinsic",
                                           "pv",      "investment", "transaction", "stage", "post",       "pre"};
    for (int i = 0; i < t.size(); ++i) {
        if (rl_money_words().contains(t.at(i)))
            return true;
        if ((t.at(i) == QLatin1String("value") || t.at(i) == QLatin1String("values")) && i > 0 &&
            kValueOf.contains(t.at(i - 1)))
            return true;
    }
    return false;
}

// Unitless measures whose names also contain an amount word ("interest_burden",
// "cash_conversion_cycle", "times_interest_earned").
const QSet<QString>& rl_plain_words() {
    static const QSet<QString> s = {"burden",  "coverage", "quality",    "proxy",       "cycle",  "intensity",
                                    "efficiency", "accruals", "times",   "elasticity",  "sensitivity", "beta",
                                    "correlation", "zscore", "statistic", "stat",        "persistence"};
    return s;
}

const QSet<QString>& rl_count_words() {
    static const QSet<QString> s = {
        "n",         "num",       "count",      "counts",     "number",     "paths",      "simulations",
        "sims",      "iterations", "iter",      "trials",     "observations", "obs",      "periods",
        "period",    "days",      "months",     "nodes",      "size",       "rank",       "dof",
        "df",        "lags",      "lag",        "window",     "degree",     "order",      "dim",
        "dimension", "dims",      "points",     "components", "clusters",   "holdings",   "deals",
        "comps",     "years",     "steps",      "shares",     "samples",    "seed",       "candidates",
        "scenarios", "methods",   "factors",    "phase",      "assets",     "contracts",  "units"};
    return s;
}

const QSet<QString>& rl_signed_words() {
    static const QSet<QString> s = {"pnl",    "npv",    "excess",     "alpha",  "theta",     "change",
                                    "difference", "diff", "gain",     "gains",  "profit",    "accretion",
                                    "upside", "surplus", "imbalance", "return", "returns",   "irr",
                                    "cagr",   "added",  "vs",         "outperformance", "carry", "drag"};
    return s;
}

const QSet<QString>& rl_acronyms() {
    static const QSet<QString> s = {
        "npv",  "irr",  "var",   "cvar", "es",   "pv",   "fv",   "ytm",  "ev",   "ebitda", "ebit", "eps",
        "roe",  "roa",  "roic",  "roce", "wacc", "cagr", "moic", "dcf",  "lbo",  "cds",    "pd",   "lgd",
        "ead",  "nav",  "aum",   "dpi",  "tvpi", "rvpi", "gdp",  "cpi",  "fx",   "id",     "ols",  "pca",
        "garch", "arima", "aic", "bic",  "mse",  "rmse", "mae",  "auc",  "roc",  "ks",     "psi",  "csi",
        "tips", "sri",  "eia",   "noi",  "ltv",  "cac",  "arr",  "nrr",  "gmv",  "cet1",   "nim",  "npl",
        "npa",  "usd",  "cva",   "dva",  "fva",  "xva",  "pfe",  "epe",  "ene",  "mtm",    "oas",  "irs",
        "ois",  "sofr", "fcf",   "fcff", "fcfe", "nopat", "capex", "nwc", "sga",  "pe",     "pb",   "ps",
        "peg",  "ptbv", "cvr",   "esg",  "etf",  "atm",  "otm",  "itm",  "dte",  "iv",     "hv",   "gp",
        "svi",  "sabr", "cev",   "cir",  "hjm",  "lmm",  "ou",   "gbm",  "mc",   "qmc",    "pde",  "ode",
        "sde",  "lsm",  "ytc",   "ytw",  "mle",  "kpi",  "ttm",  "ltm",  "yoy",  "qoq",    "cogs", "dso",
        "dio",  "dpo",  "ccc",   "r2",   "ai",   "ml",   "rnpv", "ipo",  "cfo",  "sec",    "us",   "uk",   "ebt",
        "eu",   "em",   "hy",    "ig",   "otc",  "vwap", "twap", "rsi",  "macd", "dv01",   "pv01", "cs01"};
    return s;
}

bool rl_is_numeric_key(const QString& key) {
    bool ok = false;
    key.toDouble(&ok);
    return ok;
}

bool rl_is_date_text(const QString& s) {
    if (s.size() < 8 || s.size() > 32)
        return false;
    return QDate::fromString(s.left(10), Qt::ISODate).isValid();
}

double rl_date_ms(const QString& s) {
    const QDate d = QDate::fromString(s.left(10), Qt::ISODate);
    if (!d.isValid())
        return kRlNaN;
    return double(QDateTime(d, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch());
}

// Keys that are labels of a statistic ("p25", "median", "low") take their unit
// from the parent key ("premium_comparison" -> percent).
bool rl_is_statistic_key(const QString& key) {
    static const QRegularExpression kStat(
        QStringLiteral("^(p\\d+|q\\d|median|mean|avg|average|min|max|low|high|mid|midpoint|base|target|lower|upper|"
                       "first|last|start|end|current|value|\\d.*)$"));
    return kStat.match(key.toLower()).hasMatch();
}

QString rl_join(const QString& path, const QString& key) {
    return path.isEmpty() ? key : path + QLatin1Char('.') + key;
}

// ── Numbers ──────────────────────────────────────────────────────────────────

int rl_used_decimals(double v, int max_d, int min_d) {
    const QString s = QString::number(v, 'f', max_d);
    const int dot = int(s.indexOf(QLatin1Char('.')));
    if (dot < 0)
        return min_d;
    int end = int(s.size());
    while (end > dot + 1 && s.at(end - 1) == QLatin1Char('0'))
        --end;
    return std::max(min_d, end - dot - 1);
}

QString rl_smart(double v, bool sign) {
    if (!std::isfinite(v))
        return fmt::placeholder();
    const double a = std::fabs(v);
    if (a == 0.0)
        return QStringLiteral("0");
    if (a >= 1e15 || a < 1e-6)
        return (sign && v > 0 ? QStringLiteral("+") : QString()) + QString::number(v, 'g', 4);
    int d = 0;
    if (std::floor(v) != v) {
        int max_d = 2;
        if (a >= 1000)
            max_d = 2;
        else if (a >= 100)
            max_d = 3;
        else if (a >= 1)
            max_d = 4;
        else
            max_d = std::min(8, 3 - int(std::floor(std::log10(a))));
        d = rl_used_decimals(v, max_d, 2);
    }
    return sign ? fmt::signed_number(v, d) : fmt::number(v, d);
}

int rl_pct_decimals(double pct) {
    const double a = std::fabs(pct);
    if (a >= 100)
        return 1;
    if (a >= 0.1 || a == 0.0)
        return 2;
    return 3;
}

// ── JSON helpers ─────────────────────────────────────────────────────────────

bool rl_same(const QJsonValue& a, const QJsonValue& b) {
    if (a.isDouble() && b.isDouble()) {
        const double x = a.toDouble(), y = b.toDouble();
        return std::fabs(x - y) <= 1e-9 * std::max(1.0, std::fabs(x));
    }
    if (a.isString() && b.isString())
        return a.toString() == b.toString();
    if (a.isBool() && b.isBool())
        return a.toBool() == b.toBool();
    if (a.isArray() && b.isArray()) {
        const QJsonArray x = a.toArray(), y = b.toArray();
        if (x.size() != y.size())
            return false;
        for (int i = 0; i < x.size(); ++i)
            if (!rl_same(x.at(i), y.at(i)))
                return false;
        return true;
    }
    return false;
}

bool rl_is_scalar(const QJsonValue& v) {
    return !v.isArray() && !v.isObject();
}

enum class ArrKind { Empty, Num, Str, Pairs, Matrix, Objects, Bools, Mixed };

ArrKind rl_classify(const QJsonArray& a) {
    if (a.isEmpty())
        return ArrKind::Empty;
    bool all_num = true, any_num = false, all_str = true, all_obj = true, all_bool = true, all_arr = true;
    for (const QJsonValue& e : a) {
        const bool num = e.isDouble() || e.isNull();
        all_num = all_num && num;
        any_num = any_num || e.isDouble();
        all_str = all_str && e.isString();
        all_obj = all_obj && e.isObject();
        all_bool = all_bool && e.isBool();
        all_arr = all_arr && e.isArray();
    }
    if (all_num)
        return any_num ? ArrKind::Num : ArrKind::Mixed;
    if (all_str)
        return ArrKind::Str;
    if (all_obj)
        return ArrKind::Objects;
    if (all_bool)
        return ArrKind::Bools;
    if (all_arr) {
        bool pairs = a.size() >= 3;
        for (const QJsonValue& r : a) {
            const QJsonArray row = r.toArray();
            for (const QJsonValue& e : row)
                if (!e.isDouble() && !e.isNull())
                    return ArrKind::Mixed;
            pairs = pairs && row.size() == 2;
        }
        return pairs ? ArrKind::Pairs : ArrKind::Matrix;
    }
    return ArrKind::Mixed;
}

QVector<double> rl_numbers(const QJsonArray& a) {
    QVector<double> out;
    out.reserve(a.size());
    for (const QJsonValue& e : a)
        out.append(e.isDouble() ? e.toDouble() : kRlNaN);
    return out;
}

double rl_magnitude(const QVector<double>& v) {
    double m = 0;
    for (double x : v)
        if (std::isfinite(x))
            m = std::max(m, std::fabs(x));
    return m;
}

bool rl_monotonic(const QVector<double>& v) {
    if (v.size() < 3)
        return false;
    bool up = true, down = true;
    for (int i = 1; i < v.size(); ++i) {
        if (!std::isfinite(v[i]) || !std::isfinite(v[i - 1]))
            return false;
        up = up && v[i] > v[i - 1];
        down = down && v[i] < v[i - 1];
    }
    return up || down;
}

bool rl_distinct(const QVector<double>& v) {
    QVector<double> s = v;
    std::sort(s.begin(), s.end());
    for (int i = 1; i < s.size(); ++i)
        if (!std::isfinite(s[i]) || s[i] == s[i - 1])
            return false;
    return !s.isEmpty() && std::isfinite(s.first());
}

QVector<QVector<double>> rl_matrix(const QJsonArray& a, int* cols) {
    QVector<QVector<double>> m;
    int c = 0;
    for (const QJsonValue& r : a) {
        QVector<double> row = rl_numbers(r.toArray());
        c = std::max(c, int(row.size()));
        m.append(row);
    }
    for (auto& row : m)
        while (row.size() < c)
            row.append(kRlNaN);
    *cols = c;
    return m;
}

// Non-negative parts that add up to 1 (fractions) or 100 (percent): weights.
bool rl_parts_of_whole(const QVector<double>& v) {
    if (v.size() < 2 || v.size() > 8)
        return false;
    double sum = 0;
    for (double x : v) {
        if (!std::isfinite(x) || x < 0)
            return false;
        sum += x;
    }
    return std::fabs(sum - 1.0) <= 0.02 || std::fabs(sum - 100.0) <= 2.0;
}

bool rl_has_word(const QString& key, const QSet<QString>& words) {
    return rl_any(rl_tokens(key), words);
}

const QSet<QString>& rl_whole_words() {
    static const QSet<QString> s = {"weight", "weights", "allocation", "allocations", "mix",       "split",
                                    "composition", "breakdown", "ownership", "distribution", "share",
                                    "shares", "proportions", "exposures", "contribution", "contributions"};
    return s;
}

const QSet<QString>& rl_sample_words() {
    static const QSet<QString> s = {"sample",   "samples",   "draws",     "final",      "terminal",  "simulated",
                                    "simulations", "outcomes", "pnl",     "pnls",       "losses",    "winsorized",
                                    "bootstrap", "residuals", "distribution", "scenarios_pnl", "payoffs"};
    return s;
}

const QSet<QString>& rl_bar_words() {
    static const QSet<QString> s = {"weights",    "weight",     "allocation", "allocations", "contributions",
                                    "contribution", "coefficients", "coefficient", "importances", "importance",
                                    "gradient",   "eigenvalues", "explained",  "loadings",   "betas",
                                    "scores",     "level",      "levels",     "exposures",  "attribution"};
    return s;
}

// Keys that name a chart x axis (numeric arrays / table columns).
bool rl_is_x_key(const QString& key, const Options& o, bool frontier) {
    static const QSet<QString> kX = {
        "times",   "time",     "t",        "tenors",   "tenor",    "x",        "x_data",   "query_x",
        "query_tenors", "strikes", "strike", "maturities", "maturity", "grid",  "t_grid",   "time_grid",
        "years",   "year",     "steps",    "step",     "periods",  "period",   "expiries", "expiry",
        "horizons", "horizon", "days",     "months",   "month",    "moneyness", "quantiles", "percentiles",
        "thresholds", "lags",  "lag",      "windows",  "x_values", "x_grid",   "s_grid",   "spot_grid",
        "spots",   "dates",    "date",     "quarter",  "quarters", "age",      "ages",     "k",
        "iteration", "iterations", "epoch", "epochs", "levels_x", "maturity_years", "terms", "term"};
    const QString k = key.toLower();
    if (kX.contains(k) || o.x_keys.contains(key))
        return true;
    return frontier && (k == QLatin1String("volatility") || k == QLatin1String("risk") || k == QLatin1String("std"));
}

bool rl_is_label_key(const QString& key) {
    static const QSet<QString> s = {"labels", "names",  "assets",  "tickers",  "symbols", "categories", "buckets",
                                    "methods", "factors", "features", "columns", "index",  "series",     "scenarios",
                                    "sectors", "industries", "segments", "tenor_labels", "classes", "regimes"};
    return s.contains(key.toLower());
}

bool rl_is_caption_key(const QString& key) {
    static const QSet<QString> s = {"note",        "notes",      "insight",     "insights",   "conclusion",
                                    "interpretation", "description", "message", "summary",    "explanation",
                                    "rationale",   "comment",    "methodology", "warning",    "caveat",
                                    "disclaimer",  "assumption", "commentary",  "analysis",   "reasoning",
                                    "variant",     "model_note", "bottom_line", "recommendation_text"};
    const QStringList t = rl_tokens(key);
    return !t.isEmpty() && (s.contains(t.last()) || s.contains(key.toLower()));
}

// Short texts that are a verdict and belong among the headline KPIs.
bool rl_is_verdict_key(const QString& key) {
    static const QSet<QString> s = {"signal",   "recommendation", "rating",     "verdict",  "decision",
                                    "status",   "regime",         "classification", "category", "zone",
                                    "assessment", "action",       "direction",  "trend",    "result",
                                    "conclusion", "quality",      "confidence", "position", "stance"};
    const QStringList t = rl_tokens(key);
    return !t.isEmpty() && (s.contains(t.last()) || s.contains(t.first()));
}

int rl_headline_rank(const QString& key) {
    static const QStringList kOrder = {
        "price",     "npv",       "fair_value", "present_value", "value",    "valuation",  "equity_value",
        "enterprise_value", "irr", "moic",     "ytm",           "yield",    "var",        "cvar",
        "expected_shortfall", "es", "rate",    "volatility",    "vol",      "sharpe_ratio", "sharpe",
        "duration",  "convexity", "delta",     "gamma",         "vega",     "theta",      "rho",
        "estimate",  "result",    "mean",      "std",           "score",    "statistic",  "probability",
        "total",     "spread",    "return",    "pv",            "roe",      "roic",       "net_margin",
        "gross_margin", "operating_margin", "revenue_growth", "earnings_growth", "pe_ratio", "ev_ebitda",
        "debt_to_equity", "current_ratio"};
    const QString k = key.toLower();
    const int exact = int(kOrder.indexOf(k));
    if (exact >= 0)
        return exact;
    const QStringList t = rl_tokens(key);
    for (int i = 0; i < kOrder.size(); ++i)
        if (t.contains(kOrder.at(i)))
            return 100 + i;
    return 1000;
}

// ── Gauges for known scores ──────────────────────────────────────────────────

std::optional<Block> rl_gauge(const QString& parent_key, const QString& key, double v, const QString& path) {
    if (!std::isfinite(v))
        return std::nullopt;
    const QString p = parent_key.toLower(), k = key.toLower();
    Block g;
    g.kind = Kind::Gauge;
    g.key = path;
    g.value = v;
    if ((p.contains(QLatin1String("altman")) && k == QLatin1String("z_score")) || k == QLatin1String("altman_z_score")) {
        g.title = rl_tr("Altman Z-Score");
        g.gauge_min = 0;
        g.gauge_max = 5;
        g.bands = {{0, 1.81, -1, rl_tr("Distress")}, {1.81, 2.99, 0, rl_tr("Grey zone")}, {2.99, 5, 1, rl_tr("Safe")}};
        return g;
    }
    if ((p.contains(QLatin1String("piotroski")) && k == QLatin1String("f_score")) ||
        k == QLatin1String("piotroski_f_score")) {
        g.title = rl_tr("Piotroski F-Score");
        g.gauge_min = 0;
        g.gauge_max = 9;
        g.bands = {{0, 3.5, -1, rl_tr("Weak")}, {3.5, 6.5, 0, rl_tr("Average")}, {6.5, 9, 1, rl_tr("Strong")}};
        return g;
    }
    if ((p.contains(QLatin1String("beneish")) && k == QLatin1String("m_score")) || k == QLatin1String("beneish_m_score")) {
        g.title = rl_tr("Beneish M-Score");
        g.gauge_min = -4;
        g.gauge_max = 0;
        g.bands = {{-4, -2.22, 1, rl_tr("Unlikely manipulator")},
                   {-2.22, -1.78, 0, rl_tr("Grey zone")},
                   {-1.78, 0, -1, rl_tr("Likely manipulator")}};
        return g;
    }
    if (k.endsWith(QLatin1String("quality_score")) && !k.startsWith(QLatin1String("average")) &&
        !k.startsWith(QLatin1String("avg")) && v >= 0 && v <= 100) {
        g.title = humanize(key);
        g.gauge_min = 0;
        g.gauge_max = 100;
        g.bands = {{0, 40, -1, rl_tr("Weak")}, {40, 70, 0, rl_tr("Fair")}, {70, 100, 1, rl_tr("Strong")}};
        return g;
    }
    return std::nullopt;
}

// ── Builder ──────────────────────────────────────────────────────────────────

struct NumArr {
    QString key;
    QString path;
    QVector<double> v;
    bool echo = false;
    bool used = false;
};
struct StrArr {
    QString key;
    QString path;
    QStringList v;
    bool used = false;
};
struct MatArr {
    QString key;
    QString path;
    QVector<QVector<double>> m;
    int cols = 0;
    bool pairs = false;
};

class Builder {
  public:
    explicit Builder(const Options& o) : o_(o) {}

    Block build(const QJsonValue& payload);

  private:
    Field make_field(const QString& path, const QString& key, const QJsonValue& v, const QString& parent_key) const;
    Block plan_object(const QJsonObject& obj, const QString& path, const QString& key, int depth);
    void plan_object_array(const QJsonArray& arr, const QString& path, const QString& key, int depth,
                           std::vector<Block>& charts, std::vector<Block>& tables);
    void plan_object_map(const QJsonObject& obj, const QString& path, const QString& key, int depth,
                         std::vector<Block>& charts, std::vector<Block>& tables);
    void plan_rows(const QVector<QJsonObject>& rows, const QStringList& row_keys, const QString& path,
                   const QString& key, int depth, std::vector<Block>& charts, std::vector<Block>& tables);
    void plan_num_map(const QJsonObject& obj, const QString& path, const QString& key, int depth,
                      std::vector<Block>& charts, std::vector<Block>& sections);
    void plan_series_group(QVector<NumArr*> ys, const QString& x_key, const QVector<double>& xs,
                           const QStringList& labels, bool dates, bool bars, std::vector<Block>& out,
                           const QString& path, const QString& parent_key);
    QStringList labels_for(int n, const QVector<StrArr>& strs, const QString& prefer = QString());
    bool is_obj_map(const QJsonObject& obj) const;
    Unit unit_of(const QString& path, const QString& key, double v, const QString& parent_key) const {
        return unit_for(o_, path, key, v, parent_key);
    }
    QJsonValue resolve(const QJsonValue& root, const QString& path) const;

    const Options& o_;
    QSet<QString> promoted_;
};

Field Builder::make_field(const QString& path, const QString& key, const QJsonValue& v,
                          const QString& parent_key) const {
    Field f;
    f.path = path;
    f.key = key;
    f.label = humanize(key);
    f.value = v;
    if (v.isDouble() || v.isNull()) {
        f.unit = unit_of(path, key, v.isDouble() ? v.toDouble() : kRlNaN, parent_key);
        f.signed_result = v.isDouble() && is_signed_result_key(key);
    } else {
        f.unit = Unit::Text;
    }
    return f;
}

QJsonValue Builder::resolve(const QJsonValue& root, const QString& path) const {
    // Keys may contain dots ("7.0"): take the longest prefix that exists at each level.
    QJsonValue cur = root;
    QStringList rest = path.split(QLatin1Char('.'));
    while (!rest.isEmpty()) {
        if (!cur.isObject())
            return QJsonValue(QJsonValue::Undefined);
        const QJsonObject obj = cur.toObject();
        bool found = false;
        for (int n = int(rest.size()); n >= 1; --n) {
            const QString k = rest.mid(0, n).join(QLatin1Char('.'));
            if (obj.contains(k)) {
                cur = obj.value(k);
                rest = rest.mid(n);
                found = true;
                break;
            }
        }
        if (!found)
            return QJsonValue(QJsonValue::Undefined);
    }
    return cur;
}

bool Builder::is_obj_map(const QJsonObject& obj) const {
    if (obj.isEmpty())
        return false;
    bool numeric_keys = true;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (!it.value().isObject())
            return false;
        numeric_keys = numeric_keys && rl_is_numeric_key(it.key());
    }
    if (obj.size() < 2 && !numeric_keys)
        return false;
    QSet<QString> first;
    bool have_first = false;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QJsonObject child = it.value().toObject();
        QSet<QString> keys;
        int scalars = 0, nested = 0;
        for (auto c = child.constBegin(); c != child.constEnd(); ++c) {
            keys.insert(c.key());
            if (rl_is_scalar(c.value()))
                ++scalars;
            else
                ++nested;
        }
        if (scalars == 0 || nested > scalars)
            return false;
        if (!have_first) {
            first = keys;
            have_first = true;
            continue;
        }
        const qsizetype common = QSet<QString>(keys).intersect(first).size();
        if (double(common) < 0.5 * double(std::max(keys.size(), first.size())))
            return false;
    }
    return true;
}

QStringList Builder::labels_for(int n, const QVector<StrArr>& strs, const QString& prefer) {
    for (const auto& s : strs)
        if (s.v.size() == n && (s.key == prefer || rl_is_label_key(s.key)))
            return s.v;
    if (o_.series_labels.size() == n)
        return o_.series_labels;
    QStringList out;
    for (int i = 0; i < n; ++i)
        out << QString::number(i + 1);
    return out;
}

// One or more charts for series sharing an x axis: split by magnitude so a
// small series is never flattened onto zero next to a large one.
void Builder::plan_series_group(QVector<NumArr*> ys, const QString& x_key, const QVector<double>& xs,
                                const QStringList& labels, bool dates, bool bars, std::vector<Block>& out,
                                const QString& path, const QString& parent_key) {
    if (ys.isEmpty())
        return;
    QHash<const NumArr*, int> source_order;
    for (int i = 0; i < ys.size(); ++i)
        source_order.insert(ys[i], i);
    std::stable_sort(ys.begin(), ys.end(),
                     [](const NumArr* a, const NumArr* b) { return rl_magnitude(a->v) > rl_magnitude(b->v); });
    const double ratio = bars ? kRlBarMagnitudeRatio : kRlLineMagnitudeRatio;
    const int cap = bars ? kRlMaxBarSeries : kRlMaxLineSeries;
    QVector<QVector<NumArr*>> groups;
    for (NumArr* y : ys) {
        const double m = rl_magnitude(y->v);
        const Unit u = unit_of(rl_join(path, y->key), y->key, m, parent_key);
        bool placed = false;
        for (auto& g : groups) {
            const double top = rl_magnitude(g.first()->v);
            const Unit gu = unit_of(rl_join(path, g.first()->key), g.first()->key, top, parent_key);
            if (g.size() < cap && gu == u && (m == 0 || top / std::max(m, 1e-300) <= ratio)) {
                g.append(y);
                placed = true;
                break;
            }
        }
        if (!placed)
            groups.append({y});
    }
    // Charts follow the payload's order (the first series' chart first).
    auto first_index = [&source_order](const QVector<NumArr*>& g) {
        int m = INT_MAX;
        for (const NumArr* y : g)
            m = std::min(m, source_order.value(y, INT_MAX));
        return m;
    };
    std::stable_sort(groups.begin(), groups.end(),
                     [&first_index](const QVector<NumArr*>& a, const QVector<NumArr*>& b) {
                         return first_index(a) < first_index(b);
                     });
    for (const auto& g : groups) {
        Block b;
        b.kind = bars ? Kind::Bars : Kind::Line;
        b.key = rl_join(path, g.first()->key);
        b.x_title = x_key.isEmpty() ? (bars ? QString() : rl_tr("Index")) : humanize(x_key);
        b.x_dates = dates;
        b.xs = xs;
        b.categories = labels;
        b.x_unit = x_key.isEmpty() ? Unit::Integer : unit_of(rl_join(path, x_key), x_key, rl_magnitude(xs), parent_key);
        if (b.x_unit == Unit::Money)
            b.x_unit = Unit::Plain; // an axis of strikes / spots reads as numbers
        QStringList names;
        for (NumArr* y : g) {
            y->used = true;
            Series s;
            s.key = y->key;
            s.name = humanize(y->key);
            s.values = y->v;
            s.unit = unit_of(rl_join(path, y->key), y->key, rl_magnitude(y->v), parent_key);
            b.series.append(s);
            names << s.name;
        }
        b.title = g.size() == 1 ? names.first() : names.join(QStringLiteral(" · "));
        const QString lk = g.first()->key.toLower();
        b.diagonal = lk.contains(QLatin1String("roc")) || lk.contains(QLatin1String("cap_curve")) ||
                     lk.contains(QLatin1String("lorenz"));
        out.push_back(b);
    }
}

void Builder::plan_rows(const QVector<QJsonObject>& rows, const QStringList& row_keys, const QString& path,
                        const QString& key, int depth, std::vector<Block>& charts, std::vector<Block>& tables) {
    const bool frontier = key.toLower().contains(QLatin1String("frontier"));
    const int scan = std::min<int>(int(rows.size()), 500);

    // Columns in first-appearance order; nested small objects / short numeric
    // arrays are expanded one level so their values are not lost in a cell.
    QStringList cols;
    QHash<QString, QString> col_label;
    auto add_col = [&](const QString& c, const QString& label) {
        if (!cols.contains(c)) {
            cols << c;
            col_label.insert(c, label);
        }
    };
    if (!row_keys.isEmpty())
        add_col(QStringLiteral("\x1f" "key"), rl_is_numeric_key(row_keys.value(0)) ? QStringLiteral("#") : rl_tr("Item"));
    for (int r = 0; r < scan; ++r) {
        const QJsonObject& o = rows.at(r);
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            const QString k = it.key();
            if (o_.hidden.contains(k))
                continue;
            const QJsonValue v = it.value();
            if (v.isObject()) {
                const QJsonObject sub = v.toObject();
                bool small = sub.size() <= 8;
                for (auto s = sub.constBegin(); small && s != sub.constEnd(); ++s)
                    small = rl_is_scalar(s.value());
                if (small) {
                    for (auto s = sub.constBegin(); s != sub.constEnd(); ++s)
                        add_col(k + QLatin1Char('.') + s.key(), humanize(k) + QStringLiteral(" · ") + humanize(s.key()));
                    continue;
                }
            } else if (v.isArray()) {
                const QJsonArray a = v.toArray();
                if (rl_classify(a) == ArrKind::Num && a.size() <= 12) {
                    const QStringList names = o_.series_labels.size() == a.size() ? o_.series_labels : QStringList();
                    for (int i = 0; i < a.size(); ++i)
                        add_col(k + QLatin1Char('[') + QString::number(i) + QLatin1Char(']'),
                                humanize(k) + QStringLiteral(" · ") + (names.isEmpty() ? QString::number(i + 1) : names.at(i)));
                    continue;
                }
            }
            add_col(k, humanize(k));
        }
    }
    // Label-like columns first.
    static const QStringList kFront = {"name", "label", "ticker", "symbol", "method", "scenario", "metric",
                                       "date", "year",  "period", "tenor",  "target_name", "acquirer_name"};
    std::stable_sort(cols.begin(), cols.end(), [](const QString& a, const QString& b) {
        auto rank = [](const QString& c) {
            if (c.startsWith(QLatin1Char('\x1f')))
                return -1;
            const int i = int(kFront.indexOf(c.toLower()));
            return i < 0 ? 1000 : i;
        };
        return rank(a) < rank(b);
    });

    auto cell = [&](int r, const QString& c) -> QJsonValue {
        if (c.startsWith(QLatin1Char('\x1f')))
            return QJsonValue(row_keys.value(r));
        const QJsonObject& o = rows.at(r);
        if (o.contains(c))
            return o.value(c);
        const qsizetype br = c.indexOf(QLatin1Char('['));
        if (br > 0 && c.endsWith(QLatin1Char(']'))) {
            const int i = c.mid(br + 1, c.size() - br - 2).toInt();
            return o.value(c.left(br)).toArray().at(i);
        }
        const qsizetype dot = c.indexOf(QLatin1Char('.'));
        if (dot > 0)
            return o.value(c.left(dot)).toObject().value(c.mid(dot + 1));
        return QJsonValue(QJsonValue::Undefined);
    };

    Block t;
    t.kind = Kind::Table;
    t.key = path;
    t.title = depth == 0 && key.isEmpty() ? QString() : humanize(key);
    t.depth = depth;
    t.total_rows = int(rows.size());
    t.columns = cols;
    for (const QString& c : cols) {
        t.column_labels << col_label.value(c);
        double sample = kRlNaN;
        for (int r = 0; r < scan && !std::isfinite(sample); ++r) {
            const QJsonValue v = cell(r, c);
            if (v.isDouble())
                sample = v.toDouble();
        }
        QString ck = c;
        const qsizetype cut = std::max(ck.lastIndexOf(QLatin1Char('.')), ck.indexOf(QLatin1Char('[')));
        if (cut > 0 && ck.indexOf(QLatin1Char('[')) > 0)
            ck = ck.left(ck.indexOf(QLatin1Char('[')));
        else if (cut > 0)
            ck = ck.mid(cut + 1);
        t.column_units << (c.startsWith(QLatin1Char('\x1f')) ? Unit::Text
                                                              : (std::isfinite(sample) ? unit_of(rl_join(path, ck), ck, sample, key)
                                                                                       : Unit::Text));
    }
    const int nrows = std::min<int>(int(rows.size()), kMaxTableRows);
    for (int r = 0; r < nrows; ++r) {
        QVector<QJsonValue> row;
        row.reserve(cols.size());
        for (const QString& c : cols)
            row.append(cell(r, c));
        t.rows.append(row);
    }

    // ── Charts from the rows ────────────────────────────────────────────────
    const int n = int(rows.size());
    auto numeric_col = [&](const QString& c, QVector<double>* out) {
        out->clear();
        int finite = 0;
        for (int r = 0; r < n; ++r) {
            const QJsonValue v = cell(r, c);
            if (v.isDouble()) {
                out->append(v.toDouble());
                ++finite;
            } else if (v.isNull() || v.isUndefined()) {
                out->append(kRlNaN);
            } else {
                return false;
            }
        }
        return finite >= 2;
    };

    // Explicit bar column(s) (caller's choice): bars per row label.
    const QStringList bar_cols = o_.bar_columns.value(key).split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (!bar_cols.isEmpty() && n >= 1 && n <= 40) {
        QVector<Series> bar_series;
        for (const QString& bc : bar_cols) {
            QVector<double> vals;
            if (!numeric_col(bc, &vals)) {
                if (!(n == 1 && cell(0, bc).isDouble()))
                    continue;
                vals = {cell(0, bc).toDouble()};
            }
            Series s;
            s.key = bc;
            s.name = col_label.value(bc, humanize(bc));
            s.values = vals;
            s.unit = unit_of(rl_join(path, bc), bc, rl_magnitude(vals), key);
            bar_series.append(s);
        }
        if (!bar_series.isEmpty()) {
            // Row labels: the row key or first text column; else the first other column's values.
            QString label_col;
            for (const QString& c : cols) {
                if (c.startsWith(QLatin1Char('\x1f')) || cell(0, c).isString()) {
                    label_col = c;
                    break;
                }
            }
            if (label_col.isEmpty())
                for (const QString& c : cols)
                    if (!bar_cols.contains(c)) {
                        label_col = c;
                        break;
                    }
            QStringList cats;
            for (int r = 0; r < n; ++r) {
                if (label_col.isEmpty()) {
                    cats << QString::number(r + 1);
                } else if (label_col.startsWith(QLatin1Char('\x1f'))) {
                    cats << humanize(row_keys.value(r));
                } else {
                    const QJsonValue lv = cell(r, label_col);
                    cats << (lv.isDouble() ? format_number(lv.toDouble(), t.column_units.value(cols.indexOf(label_col)), o_)
                                           : lv.toString());
                }
            }
            Block b;
            b.kind = Kind::Bars;
            b.key = rl_join(path, bar_cols.first());
            b.title = bar_series.size() == 1 ? bar_series.first().name : humanize(key);
            if (!label_col.isEmpty() && !label_col.startsWith(QLatin1Char('\x1f')) && !cell(0, label_col).isString())
                b.x_title = col_label.value(label_col);
            b.categories = cats;
            b.series = bar_series;
            charts.push_back(b);
        }
    }

    // Pivot: strike x expiry -> vol (surfaces returned as point lists).
    static const QSet<QString> kColAxis = {"strike", "moneyness", "delta", "k"};
    static const QSet<QString> kRowAxis = {"expiry", "maturity", "tenor", "t", "time", "expiration", "dte"};
    static const QSet<QString> kCellVal = {"vol", "volatility", "iv", "implied_vol", "implied_volatility", "sigma"};
    QString ca, ra, va;
    for (const QString& c : cols) {
        const QString lc = c.toLower();
        if (ca.isEmpty() && kColAxis.contains(lc))
            ca = c;
        else if (ra.isEmpty() && kRowAxis.contains(lc))
            ra = c;
        else if (va.isEmpty() && kCellVal.contains(lc))
            va = c;
    }
    if (!ca.isEmpty() && !ra.isEmpty() && !va.isEmpty() && n >= 4) {
        QVector<double> cv, rv, vv;
        if (numeric_col(ca, &cv) && numeric_col(ra, &rv) && numeric_col(va, &vv)) {
            QVector<double> cu = cv, ru = rv;
            std::sort(cu.begin(), cu.end());
            cu.erase(std::unique(cu.begin(), cu.end()), cu.end());
            std::sort(ru.begin(), ru.end());
            ru.erase(std::unique(ru.begin(), ru.end()), ru.end());
            if (cu.size() >= 2 && ru.size() >= 2 && cu.size() <= kRlMaxHeatmapEdge && ru.size() <= kRlMaxHeatmapEdge) {
                Block h;
                h.kind = Kind::Heatmap;
                h.key = rl_join(path, va);
                h.title = humanize(va);
                h.matrix = QVector<QVector<double>>(ru.size(), QVector<double>(cu.size(), kRlNaN));
                for (int r = 0; r < n; ++r) {
                    const int ri = int(std::lower_bound(ru.begin(), ru.end(), rv[r]) - ru.begin());
                    const int ci = int(std::lower_bound(cu.begin(), cu.end(), cv[r]) - cu.begin());
                    h.matrix[ri][ci] = vv[r];
                }
                const Options& o = o_;
                for (double x : ru)
                    h.row_labels << format_number(x, Unit::Plain, o);
                for (double x : cu)
                    h.col_labels << format_number(x, Unit::Plain, o);
                h.row_title = humanize(ra);
                h.col_title = humanize(ca);
                h.scale = 2;
                h.value_unit = unit_of(rl_join(path, va), va, rl_magnitude(vv), key);
                charts.push_back(h);
            }
        }
    }

    // Series against a real x column (year / period / tenor / date ...).
    QString x_col;
    QVector<double> xs;
    bool dates = false;
    if (!row_keys.isEmpty() && std::all_of(row_keys.begin(), row_keys.end(), rl_is_numeric_key) && n >= 2) {
        x_col = QStringLiteral("\x1f" "key");
        for (const QString& rk : row_keys)
            xs << rk.toDouble();
    }
    for (const QString& c : cols) {
        if (!x_col.isEmpty())
            break;
        if (c.startsWith(QLatin1Char('\x1f')) || !rl_is_x_key(c, o_, frontier))
            continue;
        QVector<double> v;
        if (numeric_col(c, &v) && rl_distinct(v) && n >= 2) {
            x_col = c;
            xs = v;
        } else if (n >= 2 && cell(0, c).isString() && rl_is_date_text(cell(0, c).toString())) {
            QVector<double> d;
            bool ok = true;
            for (int r = 0; r < n && ok; ++r) {
                const double ms = rl_date_ms(cell(r, c).toString());
                ok = std::isfinite(ms);
                d << ms;
            }
            if (ok && rl_distinct(d)) {
                x_col = c;
                xs = d;
                dates = true;
            }
        }
    }
    if (!x_col.isEmpty()) {
        static const QSet<QString> kSkip = {"rank", "id", "index", "count", "n"};
        QVector<NumArr> ys;
        for (const QString& c : cols) {
            // Expanded columns (weights[i], sub.field) stay in the table only.
            if (c == x_col || c.startsWith(QLatin1Char('\x1f')) || kSkip.contains(c.toLower()) ||
                c.contains(QLatin1Char('[')) || c.contains(QLatin1Char('.')))
                continue;
            static const QSet<QString> kAxisLike = {"year", "period", "tenor", "t", "time", "step", "month", "day",
                                                    "quarter", "maturity", "horizon"};
            if (kAxisLike.contains(c.toLower()))
                continue;
            QVector<double> v;
            if (numeric_col(c, &v))
                ys.append({c, rl_join(path, c), v, false, false});
        }
        QVector<NumArr*> ptrs;
        for (auto& y : ys)
            ptrs.append(&y);
        const QString xl = x_col.startsWith(QLatin1Char('\x1f')) ? QStringLiteral("period") : x_col;
        static const QSet<QString> kBarX = {"year", "period", "quarter", "month", "\x1f" "key"};
        const bool bars = n <= 15 && kBarX.contains(x_col.toLower()) && !dates;
        QStringList cats;
        if (bars)
            for (double x : xs)
                cats << format_number(x, Unit::Integer, o_);
        std::vector<Block> made;
        plan_series_group(ptrs, xl, xs, cats, dates, bars, made, path, key);
        // At most two charts per table: the rest is in the table itself.
        for (size_t i = 0; i < made.size() && i < 2; ++i) {
            // Four series per chart at most (the largest); every column stays in the table.
            if (made[i].series.size() > 4)
                made[i].series.resize(4);
            made[i].x_title = x_col.startsWith(QLatin1Char('\x1f')) ? rl_tr("Period") : humanize(x_col);
            for (Series& s : made[i].series)
                s.name = col_label.value(s.key, s.name);
            if (made[i].series.size() > 1)
                made[i].title = humanize(key);
            charts.push_back(made[i]);
        }
    }
    tables.push_back(t);
}

void Builder::plan_object_array(const QJsonArray& arr, const QString& path, const QString& key, int depth,
                                std::vector<Block>& charts, std::vector<Block>& tables) {
    QVector<QJsonObject> rows;
    rows.reserve(arr.size());
    for (const QJsonValue& v : arr)
        rows.append(v.toObject());
    plan_rows(rows, {}, path, key, depth, charts, tables);
}

void Builder::plan_object_map(const QJsonObject& obj, const QString& path, const QString& key, int depth,
                              std::vector<Block>& charts, std::vector<Block>& tables) {
    QStringList keys = obj.keys();
    if (std::all_of(keys.begin(), keys.end(), rl_is_numeric_key))
        std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) { return a.toDouble() < b.toDouble(); });
    QVector<QJsonObject> rows;
    for (const QString& k : keys)
        rows.append(obj.value(k).toObject());
    plan_rows(rows, keys, path, key, depth, charts, tables);
}

// An object whose values are all numbers.
void Builder::plan_num_map(const QJsonObject& obj, const QString& path, const QString& key, int depth,
                           std::vector<Block>& charts, std::vector<Block>& sections) {
    QStringList keys = obj.keys();
    QVector<double> vals;
    const bool numeric_keys = std::all_of(keys.begin(), keys.end(), rl_is_numeric_key);
    const bool date_keys = std::all_of(keys.begin(), keys.end(), rl_is_date_text);
    if (numeric_keys)
        std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) { return a.toDouble() < b.toDouble(); });
    for (const QString& k : keys)
        vals << (obj.value(k).isDouble() ? obj.value(k).toDouble() : kRlNaN);

    if ((numeric_keys || date_keys) && keys.size() >= 2) {
        NumArr y{key, path, vals, false, false};
        QVector<double> xs;
        QStringList cats;
        for (const QString& k : keys) {
            xs << (date_keys ? rl_date_ms(k) : k.toDouble());
            cats << k;
        }
        const bool bars = keys.size() <= 15 && !date_keys;
        plan_series_group({&y}, QString(), xs, cats, date_keys, bars, charts, path.section(QLatin1Char('.'), 0, -2), key);
        if (!charts.empty())
            charts.back().title = humanize(key);
        return;
    }
    if (rl_has_word(key, rl_whole_words()) && rl_parts_of_whole(vals)) {
        Block d;
        d.kind = Kind::Donut;
        d.key = path;
        d.title = humanize(key);
        for (const QString& k : keys)
            d.categories << humanize(k);
        Series s;
        s.key = key;
        s.name = humanize(key);
        s.values = vals;
        s.unit = unit_of(rl_join(path, keys.first()), keys.first(), vals.first(), key);
        d.series.append(s);
        charts.push_back(d);
        return;
    }
    Block sec = plan_object(obj, path, key, depth + 1);
    if (!sec.children.empty())
        sections.push_back(sec);
}

Block Builder::plan_object(const QJsonObject& obj, const QString& path, const QString& key, int depth) {
    Block sec;
    sec.kind = Kind::Section;
    sec.key = path;
    sec.title = depth == 0 ? QString() : humanize(key);
    sec.depth = depth;
    sec.collapsed = depth >= 3 || o_.collapsed_paths.contains(path);

    QVector<Field> scalars, inputs;
    std::vector<Block> gauges, captions, lists, charts, tables, sections;
    QVector<NumArr> nums;
    QVector<StrArr> strs;
    QVector<MatArr> mats;

    // Few headline numbers of its own: borrow the leading ones of a summary child.
    QVector<Field> borrowed;
    if (depth == 0 && o_.kpi_paths.isEmpty()) {
        int own = 0;
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            const QJsonValue v = it.value();
            const bool echo = o_.request.contains(it.key()) && rl_same(v, o_.request.value(it.key()));
            if (!echo && v.isDouble() && !o_.hidden.contains(it.key()))
                ++own;
        }
        static const QStringList kSummary = {"summary",  "results",  "result",   "metrics",
                                             "key_metrics", "headline", "overview", "valuation_summary"};
        for (const QString& sk : kSummary) {
            if (own >= 3)
                break;
            const QJsonValue sv = obj.value(sk);
            if (!sv.isObject() || o_.hidden.contains(sk))
                continue;
            const QJsonObject so = sv.toObject();
            QVector<Field> c;
            for (auto it = so.constBegin(); it != so.constEnd(); ++it)
                if (it.value().isDouble())
                    c.append(make_field(rl_join(sk, it.key()), it.key(), it.value(), sk));
            if (c.isEmpty())
                continue;
            std::stable_sort(c.begin(), c.end(),
                             [](const Field& a, const Field& b) { return rl_headline_rank(a.key) < rl_headline_rank(b.key); });
            const int take = std::min<int>(int(c.size()), std::max(0, o_.max_kpis - own));
            for (int i = 0; i < take; ++i) {
                borrowed.append(c[i]);
                promoted_.insert(c[i].path);
            }
            break;
        }
    }

    // Sibling objects with the same fields ("gross" / "net_of_fees", "mean" / "median"):
    // one comparison table, a row per field and a column per object.
    QSet<QString> compared;
    {
        QHash<QString, QStringList> by_fields;
        QStringList order;
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            const QString p = rl_join(path, it.key());
            if (!it.value().isObject() || o_.hidden.contains(it.key()) || o_.consumed.contains(p) ||
                o_.collapsed_paths.contains(p))
                continue;
            const QJsonObject c = it.value().toObject();
            if (c.size() < 3)
                continue;
            bool flat = true, promoted_child = false;
            for (auto cc = c.constBegin(); cc != c.constEnd(); ++cc) {
                flat = flat && rl_is_scalar(cc.value());
                promoted_child = promoted_child || promoted_.contains(rl_join(p, cc.key()));
            }
            if (!flat || promoted_child)
                continue;
            const QString sig = c.keys().join(QLatin1Char('\x1f'));
            if (!by_fields.contains(sig))
                order << sig;
            by_fields[sig].append(it.key());
        }
        for (const QString& sig : order) {
            const QStringList members = by_fields.value(sig);
            if (members.size() < 2)
                continue;
            const QJsonObject first = obj.value(members.first()).toObject();
            const QStringList fields = first.keys();
            Block t;
            t.kind = Kind::Table;
            t.key = path;
            QStringList names;
            for (const QString& m : members)
                names << humanize(m);
            t.title = names.join(QStringLiteral(" vs "));
            t.depth = depth;
            t.columns << QStringLiteral("\x1f" "key");
            t.column_labels << rl_tr("Metric");
            t.column_units << Unit::Text;
            for (const QString& m : members) {
                t.columns << m;
                t.column_labels << humanize(m);
                t.column_units << Unit::Plain;
            }
            for (const QString& f : fields) {
                double sample = kRlNaN;
                for (const QString& m : members) {
                    const QJsonValue v = obj.value(m).toObject().value(f);
                    if (v.isDouble()) {
                        sample = v.toDouble();
                        break;
                    }
                }
                t.row_keys << f;
                t.row_units << (std::isfinite(sample) ? unit_of(rl_join(rl_join(path, members.first()), f), f, sample,
                                                                members.first())
                                                      : Unit::Text);
                QVector<QJsonValue> row{QJsonValue(humanize(f))};
                for (const QString& m : members)
                    row.append(obj.value(m).toObject().value(f));
                t.rows.append(row);
            }
            t.total_rows = int(fields.size());
            tables.push_back(t);
            for (const QString& m : members)
                compared.insert(m);
        }
    }

    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString k = it.key();
        const QString p = rl_join(path, k);
        if (o_.hidden.contains(k) || o_.hidden.contains(p) || o_.consumed.contains(p) || promoted_.contains(p) ||
            compared.contains(k))
            continue;
        const QJsonValue v = it.value();
        const bool echo = depth == 0 && o_.request.contains(k) && rl_same(v, o_.request.value(k));

        if (v.isObject()) {
            const QJsonObject child = v.toObject();
            if (child.isEmpty())
                continue;
            bool all_num = true;
            for (auto c = child.constBegin(); c != child.constEnd(); ++c)
                all_num = all_num && (c.value().isDouble() || c.value().isNull());
            if (all_num && child.size() >= 2) {
                plan_num_map(child, p, k, depth, charts, sections);
            } else if (is_obj_map(child)) {
                plan_object_map(child, p, k, depth + 1, charts, tables);
            } else {
                Block s = plan_object(child, p, k, depth + 1);
                if (!s.children.empty())
                    sections.push_back(s);
            }
            continue;
        }
        if (v.isArray()) {
            const QJsonArray a = v.toArray();
            switch (rl_classify(a)) {
                case ArrKind::Empty:
                    break;
                case ArrKind::Num:
                    if (a.size() == 1)
                        scalars.append(make_field(p, k, a.at(0), key));
                    else
                        nums.append({k, p, rl_numbers(a), echo, false});
                    break;
                case ArrKind::Str: {
                    QStringList sv;
                    for (const QJsonValue& e : a)
                        sv << e.toString();
                    strs.append({k, p, sv, false});
                    break;
                }
                case ArrKind::Pairs:
                case ArrKind::Matrix: {
                    MatArr m;
                    m.key = k;
                    m.path = p;
                    m.m = rl_matrix(a, &m.cols);
                    m.pairs = rl_classify(a) == ArrKind::Pairs;
                    if (!echo)
                        mats.append(m);
                    break;
                }
                case ArrKind::Objects:
                    plan_object_array(a, p, k, depth + 1, charts, tables);
                    break;
                case ArrKind::Bools:
                case ArrKind::Mixed: {
                    Block l;
                    l.kind = Kind::List;
                    l.key = p;
                    l.title = humanize(k);
                    for (const QJsonValue& e : a)
                        l.items << (e.isString() ? e.toString() : format_value(e, Unit::Plain, o_));
                    lists.push_back(l);
                    break;
                }
            }
            continue;
        }
        // Scalar.
        if (v.isString()) {
            const QString s = v.toString();
            if (s.trimmed().isEmpty())
                continue;
            if (s.size() > 60 || (rl_is_caption_key(k) && s.size() > 24)) {
                Block c;
                c.kind = Kind::Caption;
                c.key = p;
                c.title = humanize(k);
                c.text = s;
                captions.push_back(c);
                continue;
            }
        }
        Field f = make_field(p, k, v, key);
        if (echo) {
            inputs.append(f);
            continue;
        }
        if (v.isDouble()) {
            if (auto g = rl_gauge(key, k, v.toDouble(), p)) {
                gauges.push_back(*g);
                continue;
            }
        }
        scalars.append(f);
    }

    // ── Matrices first: they may claim label arrays ─────────────────────────
    auto find_num = [&](const QString& k) -> NumArr* {
        for (auto& n : nums)
            if (n.key == k)
                return &n;
        return nullptr;
    };
    auto find_str = [&](const QString& k) -> StrArr* {
        for (auto& s : strs)
            if (s.key == k)
                return &s;
        return nullptr;
    };
    auto axis_labels = [&](NumArr* na, StrArr* sa, QStringList* out) {
        out->clear();
        if (na) {
            na->used = true;
            const Unit u = unit_of(na->path, na->key, rl_magnitude(na->v), key);
            for (double x : na->v)
                *out << format_number(x, u == Unit::Money ? Unit::Plain : u, o_);
        } else if (sa) {
            sa->used = true;
            *out = sa->v;
        }
    };
    for (const MatArr& m : mats) {
        const int R = int(m.m.size()), C = m.cols;
        const QString lk = m.key.toLower();
        const bool curve = lk.contains(QLatin1String("curve")) || lk.contains(QLatin1String("roc")) ||
                           lk.contains(QLatin1String("lorenz"));
        // Time-major path (ODE solution, multi-factor path): one row per x, a
        // few state variables as columns — unless the columns are an axis of
        // their own (a strike x expiry surface).
        NumArr* time_x = nullptr;
        for (auto& n : nums)
            if (int(n.v.size()) == R && rl_is_x_key(n.key, o_, false) && rl_monotonic(n.v)) {
                time_x = &n;
                break;
            }
        bool col_axis = o_.matrix_axes.contains(m.key);
        static const QSet<QString> kColAxisNames = {"strikes", "strike", "moneyness", "deltas"};
        for (const auto& n : nums)
            col_axis = col_axis || (int(n.v.size()) == C && kColAxisNames.contains(n.key.toLower()));
        if (time_x && C <= 10 && !col_axis && (R != C || C == 1)) {
            QVector<NumArr> cols_v;
            for (int c = 0; c < C; ++c) {
                NumArr y{m.key + QLatin1Char('[') + QString::number(c + 1) + QLatin1Char(']'), m.path, {}, false, false};
                for (const auto& row : m.m)
                    y.v << row.value(c, kRlNaN);
                cols_v.append(y);
            }
            QVector<NumArr*> ptrs;
            for (auto& y : cols_v)
                ptrs.append(&y);
            time_x->used = true;
            std::vector<Block> made;
            plan_series_group(ptrs, time_x->key, time_x->v, {}, false, false, made, path, key);
            for (auto& b : made) {
                for (Series& s : b.series)
                    s.name = humanize(m.key) + QLatin1Char(' ') + s.key.mid(s.key.indexOf(QLatin1Char('[')));
                b.title = humanize(m.key);
                charts.push_back(b);
            }
            continue;
        }
        if (m.pairs || (curve && R == 2 && C >= 3)) {
            NumArr y{m.key, m.path, {}, false, false};
            QVector<double> xs;
            if (m.pairs) {
                for (const auto& row : m.m) {
                    xs << row.value(0, kRlNaN);
                    y.v << row.value(1, kRlNaN);
                }
            } else {
                xs = m.m.at(0);
                y.v = m.m.at(1);
            }
            std::vector<Block> made;
            plan_series_group({&y}, QStringLiteral("x"), xs, {}, false, false, made, path, key);
            for (auto& b : made) {
                b.title = humanize(m.key);
                b.diagonal = curve;
                b.x_title = QString();
                charts.push_back(b);
            }
            continue;
        }
        // Simulated paths: one row per path.
        const bool pathish = lk.contains(QLatin1String("path"));
        if ((pathish || (R >= 10 && C >= 20)) && C >= 5 && R >= 2) {
            Block f;
            f.kind = Kind::Fan;
            f.key = m.path;
            f.title = humanize(m.key);
            f.paths = m.m;
            for (auto& n : nums) {
                if (int(n.v.size()) == C && rl_is_x_key(n.key, o_, false) && rl_monotonic(n.v)) {
                    f.xs = n.v;
                    f.x_title = humanize(n.key);
                    n.used = true;
                    break;
                }
            }
            if (f.xs.isEmpty()) {
                for (int i = 0; i < C; ++i)
                    f.xs << i;
                f.x_title = rl_tr("Step");
            }
            Series s;
            s.key = m.key;
            s.name = humanize(m.key);
            s.unit = unit_of(m.path, m.key, 0, key);
            f.series.append(s);
            charts.push_back(f);
            continue;
        }
        if (R <= kRlMaxHeatmapEdge && C <= kRlMaxHeatmapEdge && R >= 1 && C >= 1) {
            Block h;
            h.kind = Kind::Heatmap;
            h.key = m.path;
            h.title = humanize(m.key);
            h.matrix = m.m;
            h.value_unit = unit_of(m.path, m.key, rl_magnitude(m.m.value(0)), key);
            if (o_.matrix_axes.contains(m.key)) {
                const auto ax = o_.matrix_axes.value(m.key);
                axis_labels(find_num(ax.first), find_str(ax.first), &h.row_labels);
                axis_labels(find_num(ax.second), find_str(ax.second), &h.col_labels);
                h.row_title = humanize(ax.first);
                h.col_title = humanize(ax.second);
            } else {
                static const QStringList kRowNames = {"expiries", "expiry", "maturities", "tenors", "times",
                                                      "dates",    "years",  "horizons"};
                static const QStringList kColNames = {"strikes", "moneyness", "deltas", "strike"};
                for (const QString& rn : kRowNames) {
                    NumArr* n = find_num(rn);
                    StrArr* s = find_str(rn);
                    if ((n && int(n->v.size()) == R) || (s && int(s->v.size()) == R)) {
                        axis_labels(n && int(n->v.size()) == R ? n : nullptr, s && int(s->v.size()) == R ? s : nullptr,
                                    &h.row_labels);
                        h.row_title = humanize(rn);
                        break;
                    }
                }
                for (const QString& cn : kColNames) {
                    NumArr* n = find_num(cn);
                    StrArr* s = find_str(cn);
                    if ((n && int(n->v.size()) == C) || (s && int(s->v.size()) == C)) {
                        axis_labels(n && int(n->v.size()) == C ? n : nullptr, s && int(s->v.size()) == C ? s : nullptr,
                                    &h.col_labels);
                        h.col_title = humanize(cn);
                        break;
                    }
                }
                // A rectangular grid: sibling arrays of exactly R / C values are its axes
                // ("ebitda_variations" x "multiple_variations").
                if (R != C) {
                    auto take_axis = [&](int n, QStringList* labels, QString* title) {
                        if (!labels->isEmpty())
                            return;
                        for (auto& a : nums)
                            if (!a.used && !a.echo && int(a.v.size()) == n) {
                                axis_labels(&a, nullptr, labels);
                                *title = humanize(a.key);
                                return;
                            }
                        for (auto& s : strs)
                            if (!s.used && int(s.v.size()) == n) {
                                axis_labels(nullptr, &s, labels);
                                *title = humanize(s.key);
                                return;
                            }
                    };
                    take_axis(R, &h.row_labels, &h.row_title);
                    take_axis(C, &h.col_labels, &h.col_title);
                }
                if (R == C && h.row_labels.isEmpty() && h.col_labels.isEmpty()) {
                    QStringList names;
                    for (auto& s : strs)
                        if (int(s.v.size()) == R && rl_is_label_key(s.key)) {
                            names = s.v;
                            s.used = true;
                            break;
                        }
                    if (names.isEmpty() && o_.series_labels.size() == R)
                        names = o_.series_labels;
                    h.row_labels = names;
                    h.col_labels = names;
                }
            }
            for (int i = int(h.row_labels.size()); i < R; ++i)
                h.row_labels << QString::number(i + 1);
            for (int i = int(h.col_labels.size()); i < C; ++i)
                h.col_labels << QString::number(i + 1);
            // Scale: correlation, non-negative magnitude, or signed.
            bool unit_range = true, non_neg = true, diag_one = R == C;
            for (int r = 0; r < R; ++r)
                for (int c = 0; c < C; ++c) {
                    const double x = m.m[r][c];
                    if (!std::isfinite(x))
                        continue;
                    unit_range = unit_range && std::fabs(x) <= 1.0 + 1e-9;
                    non_neg = non_neg && x >= 0;
                    if (r == c)
                        diag_one = diag_one && std::fabs(x - 1.0) < 1e-6;
                }
            h.scale = (R == C && unit_range && diag_one) ? 1 : (non_neg ? 2 : 0);
            charts.push_back(h);
            continue;
        }
        // Too large for cells: a plain table of the rows.
        Block t;
        t.kind = Kind::Table;
        t.key = m.path;
        t.title = humanize(m.key);
        t.total_rows = R;
        t.columns << QStringLiteral("#");
        t.column_labels << QStringLiteral("#");
        t.column_units << Unit::Integer;
        const int cmax = std::min(C, 200);
        for (int c = 0; c < cmax; ++c) {
            t.columns << QString::number(c);
            t.column_labels << QString::number(c + 1);
            t.column_units << unit_of(m.path, m.key, rl_magnitude(m.m.value(0)), key);
        }
        for (int r = 0; r < std::min(R, kMaxTableRows); ++r) {
            QVector<QJsonValue> row{QJsonValue(r + 1)};
            for (int c = 0; c < cmax; ++c) {
                const double x = m.m[r].value(c, kRlNaN);
                row.append(std::isfinite(x) ? QJsonValue(x) : QJsonValue(QJsonValue::Null));
            }
            t.rows.append(row);
        }
        tables.push_back(t);
    }

    // ── Series against an x axis ────────────────────────────────────────────
    QVector<NumArr*> x_candidates;
    for (auto& n : nums)
        if (!n.used && rl_is_x_key(n.key, o_, false) && rl_monotonic(n.v))
            x_candidates.append(&n);
    std::stable_sort(x_candidates.begin(), x_candidates.end(), [](NumArr* a, NumArr* b) {
        // "x" before "times" before "strikes": order of preference is the key list.
        static const QStringList kPref = {"x", "times", "time", "t", "tenors", "tenor", "maturities", "years", "dates",
                                          "grid", "t_grid", "time_grid", "strikes", "expiries", "steps", "periods"};
        const int ia = int(kPref.indexOf(a->key.toLower())), ib = int(kPref.indexOf(b->key.toLower()));
        return (ia < 0 ? 1000 : ia) < (ib < 0 ? 1000 : ib);
    });
    QVector<StrArr*> date_x, label_x;
    for (auto& s : strs) {
        if (s.used || s.v.size() < 2)
            continue;
        if (std::all_of(s.v.begin(), s.v.end(), rl_is_date_text))
            date_x.append(&s);
        else if (rl_is_label_key(s.key) || rl_is_x_key(s.key, o_, false))
            label_x.append(&s);
    }
    // Assign every plain series to the first x of its length.
    QHash<NumArr*, QVector<NumArr*>> by_num_x;
    QHash<StrArr*, QVector<NumArr*>> by_date_x, by_label_x;
    QVector<NumArr*> x_order;
    QVector<StrArr*> dx_order, lx_order;
    for (auto& y : nums) {
        if (y.used || y.echo || x_candidates.contains(&y))
            continue;
        bool placed = false;
        for (NumArr* x : x_candidates) {
            if (x->v.size() == y.v.size()) {
                if (!by_num_x.contains(x))
                    x_order.append(x);
                by_num_x[x].append(&y);
                placed = true;
                break;
            }
        }
        for (int i = 0; !placed && i < date_x.size(); ++i) {
            if (date_x[i]->v.size() == y.v.size()) {
                if (!by_date_x.contains(date_x[i]))
                    dx_order.append(date_x[i]);
                by_date_x[date_x[i]].append(&y);
                placed = true;
            }
        }
        for (int i = 0; !placed && i < label_x.size(); ++i) {
            if (label_x[i]->v.size() == y.v.size()) {
                if (!by_label_x.contains(label_x[i]))
                    lx_order.append(label_x[i]);
                by_label_x[label_x[i]].append(&y);
                placed = true;
            }
        }
    }
    for (NumArr* x : x_order) {
        x->used = true;
        plan_series_group(by_num_x.value(x), x->key, x->v, {}, false, false, charts, path, key);
    }
    for (StrArr* x : dx_order) {
        x->used = true;
        QVector<double> ms;
        for (const QString& s : x->v)
            ms << rl_date_ms(s);
        plan_series_group(by_date_x.value(x), x->key, ms, {}, true, false, charts, path, key);
    }
    for (StrArr* x : lx_order) {
        x->used = true;
        plan_series_group(by_label_x.value(x), x->key, {}, x->v, false, true, charts, path, key);
    }

    // ── Remaining arrays without an axis ────────────────────────────────────
    QMap<int, QVector<NumArr*>> cohorts;
    for (auto& y : nums)
        if (!y.used && !y.echo)
            cohorts[int(y.v.size())].append(&y);
    for (auto it = cohorts.begin(); it != cohorts.end(); ++it) {
        const int L = it.key();
        QVector<NumArr*> group = it.value();
        // Several parallel short arrays: one table, plus one chart of the leading group.
        if (group.size() >= 3 && L <= 50) {
            Block t;
            t.kind = Kind::Table;
            t.key = path;
            t.title = depth == 0 ? rl_tr("Series") : humanize(key);
            t.total_rows = L;
            const QStringList labels = labels_for(L, strs);
            t.columns << QStringLiteral("#");
            t.column_labels << QStringLiteral("#");
            t.column_units << Unit::Text;
            for (NumArr* y : group) {
                t.columns << y->key;
                t.column_labels << humanize(y->key);
                t.column_units << unit_of(y->path, y->key, rl_magnitude(y->v), key);
            }
            for (int r = 0; r < L; ++r) {
                QVector<QJsonValue> row{QJsonValue(labels.value(r))};
                for (NumArr* y : group)
                    row.append(std::isfinite(y->v[r]) ? QJsonValue(y->v[r]) : QJsonValue(QJsonValue::Null));
                t.rows.append(row);
            }
            if (L <= 12) {
                std::vector<Block> made;
                plan_series_group(group, QString(), {}, labels, false, true, made, path, key);
                if (!made.empty())
                    charts.push_back(made.front());
            }
            for (NumArr* y : group)
                y->used = true;
            tables.push_back(t);
            continue;
        }
        QVector<NumArr*> bars, lines;
        for (NumArr* y : group) {
            const bool samples = rl_has_word(y->key, rl_sample_words()) || o_.samples_default;
            if (samples && L >= 20) {
                Block d;
                d.kind = Kind::Distribution;
                d.key = y->path;
                d.title = humanize(y->key);
                Series s;
                s.key = y->key;
                s.name = humanize(y->key);
                s.values = y->v;
                s.unit = unit_of(y->path, y->key, rl_magnitude(y->v), key);
                d.series.append(s);
                charts.push_back(d);
                y->used = true;
            } else if (rl_has_word(y->key, rl_whole_words()) && rl_parts_of_whole(y->v)) {
                Block d;
                d.kind = Kind::Donut;
                d.key = y->path;
                d.title = humanize(y->key);
                d.categories = labels_for(L, strs);
                Series s;
                s.key = y->key;
                s.name = humanize(y->key);
                s.values = y->v;
                s.unit = unit_of(y->path, y->key, rl_magnitude(y->v), key);
                d.series.append(s);
                charts.push_back(d);
                y->used = true;
            } else if (L <= 12 || rl_has_word(y->key, rl_bar_words())) {
                bars.append(y);
            } else {
                lines.append(y);
            }
        }
        if (!bars.isEmpty())
            plan_series_group(bars, QString(), {}, labels_for(L, strs), false, true, charts, path, key);
        if (!lines.isEmpty()) {
            QVector<double> idx;
            for (int i = 0; i < L; ++i)
                idx << i;
            plan_series_group(lines, QString(), idx, {}, false, false, charts, path, key);
        }
    }
    for (auto& y : nums) {
        if (!y.used && y.echo) {
            QStringList parts;
            for (int i = 0; i < y.v.size() && i < 8; ++i)
                parts << format_number(y.v[i], unit_of(y.path, y.key, y.v[i], key), o_);
            if (y.v.size() > 8)
                parts << QStringLiteral("…");
            Field f;
            f.path = y.path;
            f.key = y.key;
            f.label = humanize(y.key);
            f.value = QJsonValue(parts.join(QStringLiteral(", ")));
            f.unit = Unit::Text;
            inputs.append(f);
        }
    }
    for (auto& s : strs) {
        if (s.used)
            continue;
        if (s.v.size() == 1) {
            Block c;
            c.kind = Kind::Caption;
            c.key = s.path;
            c.title = humanize(s.key);
            c.text = s.v.first();
            captions.push_back(c);
            continue;
        }
        Block l;
        l.kind = Kind::List;
        l.key = s.path;
        l.title = humanize(s.key);
        l.items = s.v;
        lists.push_back(l);
    }

    // ── Scalars: headline tiles + details ───────────────────────────────────
    std::vector<Block> head;
    QVector<Field> details;
    if (depth == 0 && o_.kpi_paths.isEmpty()) {
        QVector<Field> cand, rest;
        for (const Field& f : scalars) {
            const bool num = f.value.isDouble() || f.value.isNull();
            const bool verdict = f.value.isString() && rl_is_verdict_key(f.key) && f.value.toString().size() <= 32;
            if (num || verdict || f.value.isBool())
                cand.append(f);
            else
                rest.append(f);
        }
        std::stable_sort(cand.begin(), cand.end(), [](const Field& a, const Field& b) {
            auto group = [](const Field& f) { return f.value.isDouble() || f.value.isNull() ? 0 : (f.value.isString() ? 1 : 2); };
            if (group(a) != group(b))
                return group(a) < group(b);
            return rl_headline_rank(a.key) < rl_headline_rank(b.key);
        });
        const int nk = std::min<int>(int(cand.size()), std::max(1, o_.max_kpis));
        if (nk > 0 || !borrowed.isEmpty()) {
            Block k;
            k.kind = Kind::Kpis;
            k.key = path;
            k.depth = depth;
            k.fields = cand.mid(0, nk) + borrowed;
            head.push_back(k);
        }
        details = cand.mid(nk) + rest;
    } else if (depth > 0 && !scalars.isEmpty() && scalars.size() <= 6) {
        Block k;
        k.kind = Kind::Kpis;
        k.key = path;
        k.depth = depth;
        k.fields = scalars;
        head.push_back(k);
    } else {
        details = scalars;
    }

    for (auto& b : head)
        sec.children.push_back(b);
    for (auto& b : gauges)
        sec.children.push_back(b);
    for (auto& b : captions)
        sec.children.push_back(b);
    for (auto& b : charts) {
        b.depth = depth;
        sec.children.push_back(b);
    }
    for (auto& b : tables)
        sec.children.push_back(b);
    if (!details.isEmpty()) {
        Block d;
        d.kind = Kind::Fields;
        d.key = path;
        d.title = depth == 0 ? rl_tr("Details") : QString();
        d.depth = depth;
        d.fields = details;
        sec.children.push_back(d);
    }
    for (auto& b : lists)
        sec.children.push_back(b);
    for (auto& b : sections)
        sec.children.push_back(b);
    if (!inputs.isEmpty()) {
        Block in;
        in.kind = Kind::Section;
        in.key = rl_join(path, QStringLiteral("__inputs"));
        in.title = rl_tr("Inputs");
        in.depth = depth + 1;
        in.collapsed = true;
        Block f;
        f.kind = Kind::Fields;
        f.key = in.key;
        f.depth = depth + 1;
        f.fields = inputs;
        in.children.push_back(f);
        sec.children.push_back(in);
    }
    return sec;
}

Block Builder::build(const QJsonValue& payload) {
    Block root;
    root.kind = Kind::Section;

    // Caller-chosen headline KPIs, resolved anywhere in the payload.
    if (!o_.kpi_paths.isEmpty() && payload.isObject()) {
        Block k;
        k.kind = Kind::Kpis;
        for (const QString& path : o_.kpi_paths) {
            const QJsonValue v = resolve(payload, path);
            if (v.isUndefined() || !rl_is_scalar(v))
                continue;
            const QString key = path.section(QLatin1Char('.'), -1);
            const QString parent = path.section(QLatin1Char('.'), -2, -2);
            k.fields.append(make_field(path, key, v, parent));
            promoted_.insert(path);
        }
        if (!k.fields.isEmpty())
            root.children.push_back(k);
    }

    if (payload.isObject()) {
        Block body = plan_object(payload.toObject(), QString(), QString(), 0);
        for (auto& b : body.children)
            root.children.push_back(b);
        return root;
    }
    if (payload.isArray()) {
        // Wrap so every array rule applies; the synthetic key never shows.
        QJsonObject wrap;
        const QString key = o_.result_label.isEmpty() ? QStringLiteral("result") : o_.result_label;
        wrap.insert(key, payload);
        Block body = plan_object(wrap, QString(), QString(), 0);
        for (auto& b : body.children) {
            if (b.title == humanize(key))
                b.title.clear();
            root.children.push_back(b);
        }
        // A bare numeric array also gets its summary statistics as KPIs.
        const QJsonArray a = payload.toArray();
        if (rl_classify(a) == ArrKind::Num && a.size() >= 2) {
            const QVector<double> v = rl_numbers(a);
            double sum = 0, lo = std::numeric_limits<double>::infinity(), hi = -lo;
            int n = 0;
            for (double x : v)
                if (std::isfinite(x)) {
                    sum += x;
                    lo = std::min(lo, x);
                    hi = std::max(hi, x);
                    ++n;
                }
            Block k;
            k.kind = Kind::Kpis;
            auto add = [&](const QString& label, double x, Unit u) {
                Field f;
                f.key = label;
                f.label = label;
                f.value = QJsonValue(x);
                f.unit = u;
                k.fields.append(f);
            };
            add(rl_tr("Count"), n, Unit::Integer);
            add(rl_tr("Mean"), n ? sum / n : kRlNaN, Unit::Plain);
            add(rl_tr("Min"), lo, Unit::Plain);
            add(rl_tr("Max"), hi, Unit::Plain);
            root.children.insert(root.children.begin(), k);
        }
        return root;
    }
    // A bare scalar.
    Block k;
    k.kind = Kind::Kpis;
    const QString key = o_.result_label.isEmpty() ? rl_tr("Result") : o_.result_label;
    Field f = make_field(key, key, payload, QString());
    f.label = key;
    k.fields.append(f);
    root.children.push_back(k);
    return root;
}

const char* rl_kind_name(Kind k) {
    switch (k) {
        case Kind::Kpis: return "KPIS";
        case Kind::Fields: return "FIELDS";
        case Kind::Caption: return "CAPTION";
        case Kind::List: return "LIST";
        case Kind::Line: return "LINE";
        case Kind::Bars: return "BARS";
        case Kind::Donut: return "DONUT";
        case Kind::Distribution: return "DISTRIBUTION";
        case Kind::Fan: return "FAN";
        case Kind::Heatmap: return "HEATMAP";
        case Kind::Gauge: return "GAUGE";
        case Kind::Table: return "TABLE";
        case Kind::Section: return "SECTION";
    }
    return "?";
}

} // namespace

// ── Public API ───────────────────────────────────────────────────────────────

Block build(const QJsonValue& payload, const Options& options) {
    Builder b(options);
    return b.build(payload);
}

Unit unit_for(const Options& options, const QString& path, const QString& key, double value,
              const QString& parent_key) {
    if (options.unit_for) {
        const Unit u = options.unit_for(path, key, value);
        if (u != Unit::Auto)
            return u;
    }
    const QString k = key.toLower();
    const QStringList t = rl_tokens(key);
    const double a = std::fabs(value);
    const bool integral = std::isfinite(value) && std::floor(value) == value;

    if (key.contains(QLatin1Char('%')) || t.contains(QLatin1String("pct")) || t.contains(QLatin1String("percent")) ||
        t.contains(QLatin1String("percentage")))
        return Unit::Percent;
    if (t.contains(QLatin1String("bps")) || t.contains(QLatin1String("bp")) || k.contains(QLatin1String("basis_point")))
        return Unit::Bps;
    if (key.contains(QLatin1String("($")) || k.contains(QLatin1String("(usd")))
        return Unit::Plain;
    if (k == QLatin1String("p_value") || t.contains(QLatin1String("pvalue")) || t.contains(QLatin1String("pvalues")) ||
        k.endsWith(QLatin1String("p_value")) || k.endsWith(QLatin1String("p_values")) || k.contains(QLatin1String("_p_value")))
        return Unit::Plain;
    if ((k == QLatin1String("year") || (!t.isEmpty() && t.last() == QLatin1String("year"))) && integral)
        return (value >= 1800 && value <= 2300) ? Unit::Year : Unit::Integer;
    if (t.contains(QLatin1String("multiple")) || t.contains(QLatin1String("multiples")) || t.contains(QLatin1String("moic")) ||
        k.startsWith(QLatin1String("ev_")) || k.contains(QLatin1String("_ev_")) || k.startsWith(QLatin1String("price_to_")) ||
        k.contains(QLatin1String("ev/")) ||
        k.contains(QLatin1String("p/e")) || k == QLatin1String("pe") || k == QLatin1String("pe_ratio") ||
        k == QLatin1String("pb_ratio") || k == QLatin1String("ps_ratio") || k == QLatin1String("peg_ratio") ||
        k == QLatin1String("pb") || k == QLatin1String("ps"))
        return Unit::Multiple;
    const bool counted = !t.isEmpty() && (t.first() == QLatin1String("n") || t.first() == QLatin1String("num") ||
                                          t.first() == QLatin1String("number") || t.contains(QLatin1String("count")));
    if (integral && (counted || (rl_any(t, rl_count_words()) && !rl_is_money_key(t))))
        return Unit::Integer;
    static const QSet<QString> kSpans = {"days", "months", "weeks", "years", "runway"};
    if (rl_any(t, kSpans))
        return integral ? Unit::Integer : Unit::Plain;
    if (k.contains(QLatin1String("value_at_risk")) && a <= 1.5)
        return Unit::Fraction;
    const bool rate_word = rl_any(t, rl_rate_words()) || k.contains(QLatin1String("cost_of_")) ||
                           k.contains(QLatin1String("risk_premium"));
    const bool not_rate = rl_any(t, rl_not_rate_words()) || k.contains(QLatin1String("discount_factor"));
    if (rate_word && !not_rate && std::isfinite(value) && a <= 1.5)
        return Unit::Fraction;
    if ((t.contains(QLatin1String("fee")) || t.contains(QLatin1String("fees"))) && std::isfinite(value) && a < 1.0)
        return Unit::Fraction;
    if (not_rate)
        return Unit::Plain;
    // A ratio of two amounts ("debt_to_equity") or a unitless measure.
    if ((k.contains(QLatin1String("_to_")) && !k.startsWith(QLatin1String("price_to_")) &&
         !k.startsWith(QLatin1String("ev_to_"))) ||
        rl_any(t, rl_plain_words()) || k.contains(QLatin1String("price_of_risk")))
        return Unit::Plain;
    if (rl_is_money_key(t) && !rate_word)
        return Unit::Money;
    // Statistic labels ("p25", "median", "1_day") take the parent's unit.
    if (!parent_key.isEmpty() && rl_is_statistic_key(key) && parent_key != key)
        return unit_for(options, QString(), parent_key, value, QString());
    return Unit::Plain;
}

QString format_number(double v, Unit unit, const Options& o, bool sign) {
    if (!std::isfinite(v))
        return fmt::placeholder();
    const double a = std::fabs(v);
    const bool integral = std::floor(v) == v && a < 1e15;
    switch (unit) {
        case Unit::Integer:
            if (integral)
                return sign ? fmt::signed_number(v, 0) : fmt::number(v, 0);
            return rl_smart(v, sign);
        case Unit::Year:
            return integral ? QString::number(qlonglong(v)) : rl_smart(v, false);
        case Unit::Percent:
            return fmt::pct(v, rl_pct_decimals(v), sign);
        case Unit::Fraction:
            return fmt::fraction_pct(v, rl_pct_decimals(v * 100.0), sign);
        case Unit::Bps:
            return fmt::bp(v, a >= 100 ? 0 : (a >= 1 ? 1 : 2), sign);
        case Unit::Money:
            if (o.money_format)
                return o.money_format(v);
            if (a >= 1)
                return sign ? fmt::signed_number(v, 2) : fmt::number(v, 2);
            return rl_smart(v, sign);
        case Unit::Multiple:
            return fmt::ratio(v, a >= 100 ? 1 : 2);
        case Unit::Auto:
        case Unit::Plain:
        case Unit::Text:
            break;
    }
    return rl_smart(v, sign);
}

QString format_value(const QJsonValue& v, Unit unit, const Options& o, bool sign) {
    if (v.isDouble())
        return format_number(v.toDouble(), unit, o, sign);
    if (v.isBool())
        return v.toBool() ? rl_tr("Yes") : rl_tr("No");
    if (v.isString())
        return v.toString();
    if (v.isArray())
        return rl_tr("%1 values").arg(v.toArray().size());
    if (v.isObject())
        return rl_tr("%1 fields").arg(v.toObject().size());
    if (v.isNull())
        return rl_tr("not finite");
    return QString();
}

QString humanize(const QString& key) {
    if (key.isEmpty())
        return key;
    // Already human ("Deal Value ($M)", "EV/Revenue", "CamelCase") or a bare number ("7.0").
    bool has_upper = false;
    for (const QChar c : key)
        has_upper = has_upper || c.isUpper();
    if (key.contains(QLatin1Char(' ')) || key.contains(QLatin1Char('/')) || key.contains(QLatin1Char('(')) ||
        rl_is_numeric_key(key) || (has_upper && !key.contains(QLatin1Char('_'))))
        return key;
    static const QRegularExpression kSplit(QStringLiteral("[_\\-\\.]+"));
    const QStringList parts = key.split(kSplit, Qt::SkipEmptyParts);
    QStringList out;
    for (const QString& p : parts) {
        const QString l = p.toLower();
        if (rl_acronyms().contains(l))
            out << l.toUpper();
        else if (p != l) // keeps its own capitals ("Senior_beginning" -> "Senior Beginning")
            out << (p.left(1).toUpper() + p.mid(1));
        else if (l == QLatin1String("pct") || l == QLatin1String("percent") || l == QLatin1String("percentage"))
            out << QStringLiteral("%");
        else if (l == QLatin1String("bps"))
            out << QStringLiteral("bps");
        else if (l == QLatin1String("vs"))
            out << QStringLiteral("vs");
        else
            out << (p.left(1).toUpper() + p.mid(1));
    }
    return out.join(QLatin1Char(' '));
}

bool is_signed_result_key(const QString& key) {
    return rl_any(rl_tokens(key), rl_signed_words());
}

QString describe(const Block& block, int indent) {
    static const char* kUnits[] = {"auto", "plain", "int", "year", "pct", "frac", "bps", "money", "x", "text"};
    QString pad(indent * 2, QLatin1Char(' '));
    QString s = pad + QString::fromLatin1(rl_kind_name(block.kind));
    if (!block.title.isEmpty())
        s += QStringLiteral(" \"%1\"").arg(block.title);
    switch (block.kind) {
        case Kind::Kpis:
        case Kind::Fields: {
            QStringList f;
            Options o;
            for (const Field& x : block.fields)
                f << QStringLiteral("%1=%2[%3]")
                         .arg(x.label, format_value(x.value, x.unit, o, x.signed_result),
                              QString::fromLatin1(kUnits[int(x.unit)]));
            s += QStringLiteral(" {") + f.join(QStringLiteral("; ")) + QLatin1Char('}');
            break;
        }
        case Kind::Line:
        case Kind::Bars:
        case Kind::Donut:
        case Kind::Distribution:
        case Kind::Fan: {
            QStringList n;
            for (const Series& x : block.series)
                n << QStringLiteral("%1(%2,%3)").arg(x.name).arg(x.values.size()).arg(QString::fromLatin1(kUnits[int(x.unit)]));
            s += QStringLiteral(" x=%1%2 n=%3 cats=%4 [%5]")
                     .arg(block.x_title, block.x_dates ? QStringLiteral("(dates)") : QString())
                     .arg(block.xs.size())
                     .arg(block.categories.size())
                     .arg(n.join(QStringLiteral(", ")));
            if (block.kind == Kind::Fan)
                s += QStringLiteral(" paths=%1x%2").arg(block.paths.size()).arg(block.paths.value(0).size());
            if (block.diagonal)
                s += QStringLiteral(" +diagonal");
            break;
        }
        case Kind::Heatmap:
            s += QStringLiteral(" %1x%2 rows=%3[%4] cols=%5[%6] scale=%7")
                     .arg(block.matrix.size())
                     .arg(block.matrix.value(0).size())
                     .arg(block.row_title, block.row_labels.mid(0, 3).join(QLatin1Char(',')), block.col_title,
                          block.col_labels.mid(0, 3).join(QLatin1Char(',')))
                     .arg(block.scale);
            break;
        case Kind::Gauge:
            s += QStringLiteral(" %1 in [%2,%3]").arg(block.value).arg(block.gauge_min).arg(block.gauge_max);
            break;
        case Kind::Table: {
            QStringList c;
            for (int i = 0; i < block.column_labels.size(); ++i)
                c << QStringLiteral("%1[%2]").arg(block.column_labels.at(i),
                                                  QString::fromLatin1(kUnits[int(block.column_units.value(i))]));
            s += QStringLiteral(" %1 rows: %2").arg(block.total_rows).arg(c.join(QStringLiteral(", ")));
            break;
        }
        case Kind::Caption:
            s += QStringLiteral(": ") + block.text.left(70);
            break;
        case Kind::List:
            s += QStringLiteral(" (%1): ").arg(block.items.size()) + block.items.mid(0, 3).join(QStringLiteral(" | ")).left(90);
            break;
        case Kind::Section:
            if (block.collapsed)
                s += QStringLiteral(" (collapsed)");
            break;
    }
    s += QLatin1Char('\n');
    for (const Block& c : block.children)
        s += describe(c, indent + 1);
    return s;
}

} // namespace fincept::ui::result
