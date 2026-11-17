// src/screens/backtesting/BacktestResultData.h
//
// Payload parsing and number formatting shared by the Backtesting result views
// (tearsheet, command charts, trades panel, saved-run comparison). Pure data:
// no widgets except the StatTile tone enum the KPI helpers return.
//
// Payload contract: scripts/Analytics/backtesting/base/result_normalizer.py.
// Return-like values are FRACTIONS (0.12 = 12 %); money is in the run's account
// currency, which is the traded instrument's currency (the engines do not
// convert); dates are "yyyy-MM-dd" (daily) or "yyyy-MM-dd HH:mm:ss" (intraday,
// exchange wall time) and are read as UTC so a chart with set_utc(true) shows
// exactly that wall time.

#pragma once

#include "ui/widgets/StatTile.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

namespace fincept::screens::btview {

inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// How a metric value is shown.
enum class Unit {
    Fraction,       ///< 0.123 -> "12.30%"
    SignedFraction, ///< 0.123 -> "+12.30%"
    PctPoints,      ///< a difference of two fractions -> "+3.10 pp"
    Ratio,          ///< 1.234 -> "1.23"
    Money,          ///< account currency, full amount
    SignedMoney,    ///< "+$1,234.50"
    Count,          ///< grouped integer
    Bars,           ///< "12 bars"
    Plain,          ///< grouped number, 2 decimals
    Text,
};

/// Colouring rule of a KPI tile.
enum class ToneRule {
    None,      ///< always neutral
    Sign,      ///< positive / negative by sign (returns, P&L - price-direction colours)
    Drawdown,  ///< magnitude: >= 25 % danger (a breached limit), >= 10 % warning
    BelowOne,  ///< < 1 warning (profit factor)
    Negative,  ///< < 0 warning (Sharpe-like: below the risk-free rate)
};

struct Metric {
    QString path;  ///< dotted path into the payload ("performance.sharpeRatio")
    QString label; ///< translated
    Unit unit = Unit::Plain;
    ToneRule tone = ToneRule::None;
    QString tip; ///< translated explanation (tooltip)
};

/// The run's account currency and the symbol it came from.
struct FmtCtx {
    QString ccy; ///< "USD", "INR", "GBp"; empty = none known
};

// ── Payload access ──────────────────────────────────────────────────────────
QJsonValue value_at(const QJsonObject& root, const QString& dotted_path);
/// The number at `dotted_path`, NaN when absent / null / not a number.
double number_at(const QJsonObject& root, const QString& dotted_path);

// ── Formatting ──────────────────────────────────────────────────────────────
QString format_value(double v, Unit unit, const FmtCtx& ctx);
QString format_json(const QJsonValue& v, Unit unit, const FmtCtx& ctx);
ui::StatTile::Tone tone_for(double v, ToneRule rule);

/// Currency the run's money is in: the first symbol's trading currency
/// (core::market::currency_for_symbol). Empty when it cannot be told.
QString run_currency(const QStringList& symbols);
/// Distinct trading currencies of `symbols` (empty entries skipped).
QStringList run_currencies(const QStringList& symbols);

/// Broad-market benchmark (Yahoo ticker) of the venue `symbol` trades on:
/// "RELIANCE.NS" -> "^NSEI", "VOD.L" -> "^FTSE", "BTC-USD" -> "BTC-USD"... Empty
/// when the venue has none in the table (the caller keeps the user's value).
QString default_benchmark_for(const QString& symbol);
/// First-launch symbol: the broad index of the user's number-locale territory
/// ("^NSEI" for en_IN, "^FTSE" for en_GB), "SPY" when there is none.
QString default_symbol_for_locale();
/// An INDICATIVE short-term risk-free rate in percent for `ccy` (policy-rate
/// level, rounded), NaN when the currency is not in the table. A starting
/// value only: the user should enter the current rate for their period.
double indicative_risk_free_pct(const QString& ccy);

// ── Time ────────────────────────────────────────────────────────────────────
/// Epoch ms (UTC) of a provider date string; NaN when it cannot be parsed.
double parse_time_ms(const QString& s);
bool is_intraday_stamp(const QString& s);

// ── Series ──────────────────────────────────────────────────────────────────
struct EquityData {
    QVector<double> t;         ///< epoch ms
    QVector<double> equity;    ///< account currency
    QVector<double> drawdown;  ///< fraction <= 0
    QVector<double> benchmark; ///< account-currency scale; empty when none
    QVector<double> returns;   ///< per-bar fraction
    bool intraday = false;
    bool empty() const { return t.size() < 2; }
};
EquityData parse_equity(const QJsonArray& equity);

/// [{date, <value_key>}] -> points (x = epoch ms, y = value * scale); bad rows skipped.
QVector<QPointF> dated_points(const QJsonArray& rows, const QString& value_key = QStringLiteral("value"),
                              double scale = 1.0);

/// Every `step`-th value (at most ~`max_points`), last value kept - for sparklines.
QVector<double> thin(const QVector<double>& v, int max_points = 160);

/// Human label of a provider key: "maxDrawdown" -> "Max Drawdown".
QString humanize_key(const QString& key);

} // namespace fincept::screens::btview
