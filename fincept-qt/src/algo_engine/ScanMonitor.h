// src/algo_engine/ScanMonitor.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "storage/repositories/ScanWatchRepository.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

class QTimer;
class QThread;

namespace fincept::algo {

class RealtimeScanRunner;

/// Background service that runs persisted ScanWatches: polls candles on a timer,
/// evaluates each watch's conditions per symbol, edge+cooldown gates, and fires
/// toast/provider alerts. Singleton; lives on the main thread.
class ScanMonitor : public QObject {
    Q_OBJECT
  public:
    static ScanMonitor& instance();

    void start();                       // load active watches, begin polling
    void reload(const QString& id);     // re-read one watch and (re)start/stop its runner
    void stop_watch(const QString& id);
    void test_fire(const QString& id, const QString& symbol);

  signals:
    void watch_fired(const QString& watch_id, const QString& symbol, const QString& detail);
    void watch_status_changed(const QString& watch_id, const QString& status);
    // Realtime-only: carries the live price for the Universe matches table. The
    // generic watch_fired (above) still drives toasts + alert history for both modes.
    void realtime_match(const QString& watch_id, const QString& symbol, double price,
                        const QString& detail);

  private:
    ScanMonitor() = default;
    Q_DISABLE_COPY(ScanMonitor)

    struct SymState { bool armed = true; qint64 last_fired_ms = 0; };
    struct Runner {
        ScanWatch watch;
        QTimer*   timer = nullptr;
        QHash<QString, SymState> states;
        RealtimeScanRunner* rt = nullptr; // non-null for mode=='realtime'
    };

    void start_watch(const ScanWatch& w);
    void poll(const QString& id);
    void on_candles(const QString& id, const QHash<QString, QVector<OhlcvCandle>>& data);
    void dispatch(const ScanWatch& w, const QString& symbol, const QString& detail);

    void start_realtime(const ScanWatch& w);
    void on_realtime_match(const QString& watch_id, const QString& symbol,
                           double price, const QString& detail);
    QStringList resolve_universe(const ScanWatch& w);

    QThread* scan_thread_ = nullptr; // lazily created; hosts all RealtimeScanRunners

    QHash<QString, Runner*> runners_;
};

} // namespace fincept::algo
