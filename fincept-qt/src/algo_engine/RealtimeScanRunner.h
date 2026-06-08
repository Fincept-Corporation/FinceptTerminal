// src/algo_engine/RealtimeScanRunner.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/CandleAggregator.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

class QTimer;

namespace fincept::trading { struct BrokerQuote; }

namespace fincept::algo {

/// Runs ONE realtime universe scan on ScanMonitor's dedicated scan thread.
/// Subscribes the whole universe to the live quote feed, keeps a per-symbol candle
/// window, and on a coalesced sweep (default 500 ms) evaluates the strategy
/// conditions on every symbol that priced since the last sweep. Emits match_found
/// on a fresh false→true edge, gated by a per-symbol cooldown. Alert-only — there is
/// NO position/order machinery here.
class RealtimeScanRunner : public QObject {
    Q_OBJECT
  public:
    RealtimeScanRunner(QString watch_id, QStringList universe, QJsonArray conditions,
                       QString logic, QString timeframe, QString broker_id,
                       QString account_id, QString data_source, int sweep_ms,
                       int cooldown_min, QObject* parent = nullptr);
    ~RealtimeScanRunner() override;

    /// Minimum bars of history the conditions need: max(indicator period,
    /// compare period) + max(offset, compare_offset) + 2 (crossing lookback),
    /// floored at 2. Recurses into nested condition groups. Pure/static — also
    /// covered by --selftest-universe-scan.
    static int required_bars(const QJsonArray& conditions);

  public slots:
    void start();                                                  // scan thread
    void stop();                                                   // scan thread
    void warm(const QString& symbol, const QVector<OhlcvCandle>& candles); // scan thread

  signals:
    void match_found(const QString& watch_id, const QString& symbol,
                     double price, const QString& detail);
    void status_changed(const QString& watch_id, const QString& status);

  private:
    struct SymbolState {
        std::unique_ptr<CandleAggregator> agg;
        double last_price = 0;
        double prev_price = 0;
        bool   armed = true;     // edge gate: fire only on false→true
        bool   dirty = false;    // priced since last sweep
        bool   warmed = false;   // has history → eligible to evaluate
        qint64 last_fired_ms = 0;
    };

    void on_quote(const QString& symbol, const fincept::trading::BrokerQuote& q);
    void on_sweep();
    QString consumer_id(const QString& symbol) const;
    QVector<OhlcvCandle> eval_window(const SymbolState& st) const;

    const QString    watch_id_;
    const QStringList universe_;
    const QJsonArray conditions_;
    const QString    logic_;
    const QString    timeframe_;
    const Timeframe  tf_enum_;
    const QString    broker_id_;
    const QString    account_id_;
    const QString    data_source_;
    const int        sweep_ms_;
    const int        cooldown_min_;
    const int        required_bars_;

    QHash<QString, SymbolState*> states_; // owned; built in start(), freed in dtor (scan thread)
    QTimer* sweep_timer_ = nullptr;
};

} // namespace fincept::algo
