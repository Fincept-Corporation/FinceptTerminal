// src/screens/backtesting/BtRunController.h
//
// Runs of the Backtesting screen: one object between the screen's panes and BacktestingService.
//   * a QUEUE: at most one heavy run (backtest, optimise, walk-forward, Monte Carlo, indicator sweep)
//     of this screen at a time, up to kMaxQueued waiting (FIFO); light research tools run beside it;
//   * progress (stderr progress lines, via the service) and elapsed time per run;
//   * cancel: a queued run is dropped, a running one cancelled through the service (cooperative flag,
//     then the process tree is killed after the grace period);
//   * results are parsed into typed models (BacktestResultModel.h) and auto-saved to BacktestRunStore
//     on a WORKER thread, then delivered as one immutable bundle on the GUI thread;
//   * saved runs are read back (payload + inputs) on a worker as the same bundle.
#pragma once

#include "services/backtesting/BacktestResultModel.h"
#include "services/backtesting/BacktestRunProtocol.h"
#include "services/backtesting/BacktestRunStore.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>

namespace fincept::screens::bt {

enum class BtKind { Backtest, Optimize, WalkForward, MonteCarlo, Research };

/// The command id the service takes for a kind ("backtest", "optimize", ...). Research: the tool id.
QString bt_command_of(BtKind kind);
BtKind bt_kind_of(const QString& command);

/// A finished (or re-opened) result with everything a view needs to state what produced it.
struct BtResultBundle {
    BtKind kind = BtKind::Backtest;
    QString engine;         ///< provider slug
    QString command;        ///< service command id
    QJsonObject request;    ///< the request it was made from (inputs)
    QString signature;      ///< run_signature(engine, command, request)
    QDateTime as_of;        ///< when it was produced (UTC)
    qint64 store_id = 0;    ///< BacktestRunStore id (0 = not saved)
    QString label;          ///< pin label
    QString currency;       ///< ISO code of the money
    QJsonObject payload;    ///< the result data (all kinds)
    services::backtest::BtRunPtr run;
    services::backtest::BtOptimizePtr optimize;
    services::backtest::BtWalkForwardPtr walk_forward;
    services::backtest::BtMonteCarloPtr monte_carlo;
    /// "s<store id>" for saved results, else "t<token>".
    QString key;
};
using BtBundlePtr = std::shared_ptr<const BtResultBundle>;

class BtRunController : public QObject {
    Q_OBJECT
  public:
    static constexpr int kMaxQueued = 3;

    explicit BtRunController(QObject* parent = nullptr);
    ~BtRunController() override;

    /// Start (or queue) a run. Returns a token (> 0), or 0 when the queue is full (failed() is not
    /// emitted then; `*why` gets the sentence). `currency` = account currency of the request's first
    /// symbol (used when the payload does not state one).
    quint64 start(BtKind kind, const QString& engine, const QString& command, const QJsonObject& request,
                  const QString& signature, const QString& currency, QString* why = nullptr);
    /// Cancel a running or queued run (cancelled() follows at once). False when unknown.
    bool cancel(quint64 token);
    /// Cancel everything of this screen (screen destroyed).
    void cancel_all();

    bool is_active(quint64 token) const;
    /// 0 = running, n = n-th in the queue, -1 = unknown / finished.
    int queue_position(quint64 token) const;
    bool heavy_busy() const { return heavy_running_ != 0; }
    int queued_count() const { return static_cast<int>(queue_.size()); }
    /// Milliseconds since the run started (not counting time in the queue); -1 when not running.
    qint64 elapsed_ms(quint64 token) const;
    /// The last progress line of a running run (phase empty when none arrived yet).
    services::backtest::BtProgress last_progress(quint64 token) const;
    /// What a running / queued token is (for the status text).
    BtKind kind_of(quint64 token) const;
    QString engine_of(quint64 token) const;

    /// A bundle delivered earlier (kept for the last kMaxCached results), or null.
    BtBundlePtr bundle(const QString& key) const;
    /// Keep a bundle (e.g. one read from the store) so panes can find it by key.
    void remember(const BtBundlePtr& bundle);

    /// Read a saved run (row + payload + inputs) on a worker and parse it; `done` on the GUI thread
    /// with the bundle, or null and an error sentence. The bundle is remembered.
    void load_saved(qint64 store_id, std::function<void(BtBundlePtr, QString)> done);
    /// The newest saved run of `command` (+ engine / strategy) — a one-row query, GUI thread.
    services::backtest::SavedRun latest_saved(const QString& command, const QString& engine = {},
                                              const QString& strategy = {}) const;

  signals:
    /// Queue / running set changed (buttons, status text).
    void state_changed();
    void started(quint64 token);
    void progress(quint64 token, const fincept::services::backtest::BtProgress& p);
    void finished(quint64 token, const fincept::screens::bt::BtBundlePtr& bundle);
    void failed(quint64 token, const fincept::services::backtest::BtOutcome& outcome);
    void cancelled(quint64 token);

  private:
    struct Job {
        quint64 token = 0;
        BtKind kind = BtKind::Backtest;
        QString engine;
        QString command;
        QJsonObject request;
        QString signature;
        QString currency;
        quint64 service_id = 0; ///< BacktestingService run id once dispatched
        QElapsedTimer clock;
        services::backtest::BtProgress last;
    };
    static bool is_heavy(const Job& j);
    void dispatch(Job job);
    void pump();
    void on_service_finished(quint64 run_id, const services::backtest::BtOutcome& outcome);
    void on_service_cancelled(quint64 run_id);
    void on_service_progress(quint64 run_id, const services::backtest::BtProgress& p);
    quint64 token_for_service(quint64 run_id) const;

    quint64 next_token_ = 1;
    quint64 heavy_running_ = 0;      ///< token of the heavy run in flight
    QVector<Job> queue_;             ///< waiting heavy runs (FIFO)
    QHash<quint64, Job> running_;    ///< token -> running job
    QHash<QString, BtBundlePtr> cache_;
    QStringList cache_order_;        ///< least recently delivered first
    static constexpr int kMaxCached = 12;
};

} // namespace fincept::screens::bt
