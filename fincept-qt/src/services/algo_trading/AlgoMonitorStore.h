// src/services/algo_trading/AlgoMonitorStore.h
#pragma once
// AlgoMonitorStore — read side of the live / paper monitoring history the
// DeploymentRunner writes: P&L snapshots (algo_equity_snapshots), fills
// (algo_trades) and the signal / order / risk log (algo_events).
//
// Plain blocking reads on the calling thread's database connection — call them
// from a worker (QtConcurrent) and hand the result to the GUI. Every function
// tolerates a database that has not run migration v057 yet (empty results /
// default columns).

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::algo {

struct AlgoEquityPoint {
    qint64 t_ms = 0;
    double equity = 0; ///< realized + unrealized net P&L
    double realized = 0;
    double unrealized = 0;
};

struct AlgoTradeRow {
    qint64 t_ms = 0;
    QString symbol;
    QString leg_symbol;
    QString side;
    QString reason;
    QString mode;
    QString broker_order_id;
    double quantity = 0;
    double price = 0;
    double ref_price = 0; ///< signal reference (slippage = price - ref_price)
    double pnl = 0;       ///< net, exit rows only
    double gross_pnl = 0;
    double fees = 0;
    qint64 latency_ms = 0;
    bool is_exit() const { return reason != QLatin1String("entry_signal"); }
};

struct AlgoEventRow {
    qint64 t_ms = 0;
    QString kind; ///< signal | order | fill | reject | risk | info | error
    QString message;
};

namespace monitor_store {

/// P&L curves of `deployment_ids` since `since_ms`, oldest first, thinned to at
/// most `max_points` per deployment (the last point is always kept).
QHash<QString, QVector<AlgoEquityPoint>> equity_curves(const QStringList& deployment_ids, qint64 since_ms,
                                                       int max_points = 1500);

/// Fills of one deployment, newest first.
QVector<AlgoTradeRow> trades(const QString& deployment_id, int limit = 1000);

/// Log lines of one deployment, newest first.
QVector<AlgoEventRow> events(const QString& deployment_id, int limit = 1000);

} // namespace monitor_store
} // namespace fincept::services::algo
