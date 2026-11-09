// src/services/equity/TechnicalIndicators.h
//
// Native technical indicators for the Equity Research Technicals tab, the Sentiment blend and the
// workflow "Technical Indicators" node. Pure and synchronous: no Qt event loop, no I/O, safe to call
// from a worker thread.
//
// compute_all() / compute_frame() reproduce the former Python implementation — the `ta` 0.11.0
// library running on pandas 2.3 — column for column: the same names, windows and parameters, the
// same warm-up conventions (including ta's placeholder zeros in the first ATR / ADX rows) and the
// same arithmetic: pandas' Kahan running sums and Welford rolling variance, its adjust=False EWM
// kernel, numpy's pairwise summation and the left-to-right evaluation order of every ta formula.
// Non-finite results (NaN, ±inf) mean "no value" and serialise as JSON null, as pandas' to_json did.
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::equity::technicals {

/// OHLCV columns of a bar series, oldest bar first. NaN marks a missing cell (JSON null, an absent
/// key or a non-numeric value).
struct PriceSeries {
    QVector<double> open;
    QVector<double> high;
    QVector<double> low;
    QVector<double> close;
    QVector<double> volume;  ///< all NaN when no bar carries a volume field
    bool has_volume = false; ///< true when at least one bar has a "volume" key
};

/// One indicator column, aligned with the bars. NaN / ±inf = no value (JSON null).
struct IndicatorColumn {
    QString name;
    QVector<double> values;
};

/// Reads bars (JSON objects, oldest first) into a PriceSeries. The lowercase OHLCV key is used, or
/// its capitalised form ("Close") when the lowercase key is absent. Fails when a bar is not an
/// object, or when none of the bars carries one of the `required` columns.
bool read_price_series(const QJsonArray& bars, const QStringList& required, PriceSeries* out, QString* error);

/// Every indicator of the Technicals frame, in frame column order: trend, momentum, volatility,
/// volume, others. PVO and the volume group are present only when the series has a volume column.
/// Where `ta` raised instead of producing output — ATR on fewer than 14 bars, ADX / ±DI on fewer
/// than 28 — that indicator's columns are all null and every other column is still computed.
QVector<IndicatorColumn> compute_all(const PriceSeries& series);

/// The Technicals frame: each input bar with all of its own fields, plus every compute_all()
/// column. Indicators always run over the whole series; `keep_last` > 0 returns only the newest
/// `keep_last` rows of the frame. Fails (ok == false, error set) when a bar is not an object or an
/// open/high/low/close column is missing.
struct TechnicalsFrame {
    bool ok = false;
    QString error;
    QJsonArray rows;
};
TechnicalsFrame compute_frame(const QJsonArray& bars, qsizetype keep_last = 0);

// ── Single indicator (workflow node) ────────────────────────────────────────────────────────────

/// Indicators compute_single() accepts, in the node's menu order.
QStringList single_indicator_names();

/// Largest lookback period compute_single() accepts.
inline constexpr int kMaxIndicatorPeriod = 10000;

struct SingleIndicatorResult {
    bool ok = false;
    QString error;
    /// {"success", "indicator", "period", "params", "columns", "value", "latest", "data"} — see
    /// compute_single().
    QJsonObject output;
};

/// Computes one indicator over `price_data` with the given lookback `period` (ignored by MACD,
/// which is always 12/26/9, and by OBV).
///
/// `price_data` is an array of bar objects, an object wrapping one under "data", "rows",
/// "candles", "bars", "history" or "prices", or column arrays ({"close": [...], "high": [...]}).
///
/// The output keeps every input bar (all of its fields) in "data" with the indicator's columns
/// added; "value" is the newest bar's main column (null when the newest bar has no value) and
/// "latest" is the newest row. Bars inside an indicator's warm-up are null — including the rows
/// where `ta` writes a placeholder 0 (ATR, ADX, ±DI) or a band flag although no band exists yet.
SingleIndicatorResult compute_single(const QJsonValue& price_data, const QString& indicator, int period);

} // namespace fincept::services::equity::technicals
