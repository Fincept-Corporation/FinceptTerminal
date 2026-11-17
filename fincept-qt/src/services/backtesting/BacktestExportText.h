// src/services/backtesting/BacktestExportText.h
//
// Text of the Backtesting exports: CSV with plain numbers (C locale, full precision, empty for a
// missing value — never a formatted "12.3%" or "$1,234") and the ISO currency code in a column, and
// result JSON wrapped with the same provenance. Every CSV starts with "# " header lines naming the
// engine (+ implementation and version), data source and adjustment, period, fill model, cost model,
// risk-free source and currency, and an unmissable SYNTHETIC line when the prices were generated.
//
// Pure QtCore: unit-tested by tests/tst_backtest_result_model.cpp.
#pragma once

#include "services/backtesting/BacktestResultModel.h"

#include <QString>
#include <QStringList>

namespace fincept::services::backtest {

struct BtExportMeta {
    QString command;          ///< backtest | optimize | walk_forward | monte_carlo
    QString engine;
    QString engine_implementation;
    QString engine_version;
    QString strategy;
    QString symbols;          ///< space separated
    QString start;
    QString end;
    QString interval;
    QString currency;         ///< ISO code
    QString data_source;
    QString adjustment;
    QString fill_model;
    QString cost_model;
    QString risk_free;
    bool synthetic = false;
    QString generated_at;     ///< ISO local time; filled by bt_export_header when empty
};

BtExportMeta bt_export_meta(const BtRun& run);
BtExportMeta bt_export_meta(const BtProvenance& prov, const BtAssumptions& a, const QString& command);
/// "# ..." lines (no trailing newline).
QStringList bt_export_header(const BtExportMeta& meta);

/// A CSV field: quoted when it contains a comma, quote or newline.
QString bt_csv_field(const QString& s);
/// A number for CSV: C locale, up to 15 significant digits; empty for NaN / inf.
QString bt_csv_number(double v);

QString bt_trades_csv(const BtRun& run, const BtExportMeta& meta);
QString bt_equity_csv(const BtRun& run, const BtExportMeta& meta);
QString bt_metrics_csv(const BtRun& run, const BtExportMeta& meta);
QString bt_drawdowns_csv(const BtRun& run, const BtExportMeta& meta);
QString bt_optimize_csv(const BtOptimizeResult& r, const BtExportMeta& meta);
QString bt_walk_forward_csv(const BtWalkForwardResult& r, const BtExportMeta& meta);
QString bt_monte_carlo_csv(const BtMonteCarloResult& r, const BtExportMeta& meta);

/// {"export": {meta...}, "result": payload} as indented JSON.
QByteArray bt_result_json(const QJsonObject& payload, const BtExportMeta& meta);

} // namespace fincept::services::backtest
