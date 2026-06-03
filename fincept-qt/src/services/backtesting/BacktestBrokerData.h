// src/services/backtesting/BacktestBrokerData.h
//
// Pre-fetches historical OHLC for a backtest from the first active Indian broker
// (via IBroker::get_history) and returns it as a JSON candle map for injection
// into the Python backtest providers. Runs the broker I/O on a worker thread.
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace fincept::services::backtest {

class BacktestBrokerData {
  public:
    /// Result: { "<CANON_SYMBOL>": [ {t,o,h,l,c,v}, ... ], ... }  (t = epoch seconds).
    /// May be empty (no broker / nothing resolved). `broker_id` is the slug used,
    /// or empty. `cb` is invoked on `context`'s thread.
    using Callback = std::function<void(QJsonObject candles, QString broker_id)>;

    /// Non-blocking. Fetches on a worker thread, then marshals `cb` onto `context`.
    static void fetch(QObject* context, const QStringList& symbols, const QString& start_date,
                      const QString& end_date, const QString& interval, Callback cb);

    /// True when at least one *active* account belongs to an Indian broker.
    static bool has_active_indian_broker();
};

} // namespace fincept::services::backtest
