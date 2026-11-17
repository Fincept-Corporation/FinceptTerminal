// src/screens/backtesting/BacktestViews.h
//
// Widget builders for the Backtesting result tabs. Free functions: each builds
// fresh widgets from one payload (see BacktestResultData.h for the contract) and
// hands them to the caller's layout, which owns and later deletes them.
// Everything is drawn from theme tokens and the shared chart widgets, so a theme
// or font change needs no rebuild (charts repaint, labels are styled by the
// screen's root stylesheet through the object names used here).
//
//   BacktestCharts.cpp     equity chart, trade chart, per-command CHART tab views

#pragma once

#include "screens/backtesting/BacktestResultData.h"

#include <QJsonObject>
#include <QString>

class QWidget;

namespace fincept::ui {
class CandleChart;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::btview {

// ── CHART tab (BacktestCharts.cpp) ──────────────────────────────────────────
/// Equity vs benchmark with a drawdown pane, range presets, LOG / % / =100
/// modes and trade entry / exit markers. nullptr when there is no curve.
/// `interactive` = wheel zoom + drag pan (off inside the scrolling summary).
ui::TimeSeriesChart* make_equity_chart(QWidget* parent, const QJsonObject& payload, const FmtCtx& ctx,
                                       bool interactive);
/// The CHART tab content for `command`; nullptr when the result has nothing to plot.
QWidget* make_command_chart(QWidget* parent, const QString& command, const QJsonObject& payload, const FmtCtx& ctx);

/// Symbols that have OHLC in payload.priceData (trade chart choices).
QStringList price_symbols(const QJsonObject& payload);
/// Load `symbol`'s bars and its trades' entry / exit markers into `chart`.
/// Returns false (and clears the chart) when there are no bars for it.
bool load_trade_chart(ui::CandleChart* chart, const QJsonObject& payload, const QString& symbol, const FmtCtx& ctx);

} // namespace fincept::screens::btview
