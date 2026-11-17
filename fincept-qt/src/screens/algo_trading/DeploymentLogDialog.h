// src/screens/algo_trading/DeploymentLogDialog.h
#pragma once
#include "services/algo_trading/AlgoMonitorStore.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QDialog>

class QLabel;
class QTabWidget;
class QTableWidget;
class QVBoxLayout;

namespace fincept::ui {
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

/// History of one deployment: P&L curve with its drawdown (realized +
/// unrealized, from algo_equity_snapshots), statistics computed with the same
/// definitions as the backtest report (algo_engine/PerformanceMetrics), every
/// fill with its fees, slippage against the signal price and broker latency
/// (algo_trades), and the signal / order / risk log (algo_events).
class DeploymentLogDialog : public QDialog {
    Q_OBJECT
  public:
    DeploymentLogDialog(const fincept::services::algo::AlgoDeployment& deployment, const QString& currency,
                        QWidget* parent = nullptr);

  private:
    void load();
    void populate(const QVector<fincept::services::algo::AlgoTradeRow>& trades,
                  const QVector<fincept::services::algo::AlgoEventRow>& events,
                  const QVector<fincept::services::algo::AlgoEquityPoint>& equity);
    QString money(double v, bool signed_value = false) const;
    /// The fills as machine-readable CSV: plain numbers, ISO 8601 UTC times and a
    /// currency column (never the on-screen text).
    void fills_export(QStringList* headers, QVector<QStringList>* rows) const;

    fincept::services::algo::AlgoDeployment dep_;
    QString currency_; ///< the deployment's quote currency; empty = unknown (no unit printed)
    // What populate() last showed, so a format change re-renders without a reload.
    QVector<fincept::services::algo::AlgoTradeRow> trades_;
    QVector<fincept::services::algo::AlgoEventRow> events_rows_;
    QVector<fincept::services::algo::AlgoEquityPoint> equity_;
    bool loaded_ = false;
    QLabel* status_ = nullptr;
    fincept::ui::KpiStrip* kpis_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    fincept::ui::TimeSeriesChart* chart_ = nullptr;
    QTableWidget* fills_ = nullptr;
    QTableWidget* events_ = nullptr;
};

} // namespace fincept::screens
