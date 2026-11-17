// src/services/backtesting/BacktestingService.h
#pragma once
#include "services/backtesting/BacktestRunProtocol.h"
#include "services/backtesting/BacktestingTypes.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QSet>

#include <atomic>
#include <memory>

namespace fincept::services::backtest {

/// Singleton service dispatching backtesting commands to Python providers.
///
/// Every execute() call is a *run* with its own id. Consumers should correlate on
/// that id via run_completed / run_failed / run_cancelled (and run_finished, which
/// carries the classified outcome): the legacy result_ready / error_occurred signals
/// carry no id, so two concurrent callers (Backtesting screen, Portfolio, the
/// workflow "Backtest Engine" node) cannot tell their results apart. The legacy
/// signals are still emitted (for every run that was not cancelled) so existing
/// consumers keep working.
///
/// A run's process is classified by classify_run_output() (BacktestRunProtocol.h)
/// OFF the GUI thread: a killed, cancelled or envelope-less process is never an
/// (empty) success. Progress lines the provider writes to stderr arrive as
/// run_progress(); they are never logged.
class BacktestingService : public QObject {
    Q_OBJECT
  public:
    static BacktestingService& instance();

    /// Execute a backtesting command on a specific provider. Returns the run id
    /// (never 0) that run_completed / run_failed / run_cancelled will carry.
    quint64 execute(const QString& provider, const QString& command, const QJsonObject& args);

    /// Cancel an in-flight run. The run's eventual result is discarded (no
    /// result_ready / run_completed is emitted for it) and run_cancelled fires
    /// immediately. Cooperative first: the provider polls a cancel-flag file between
    /// iterations and exits (code 3). If the process is still alive after
    /// kCancelGraceMs its whole process tree is killed (PythonRunner cancel token),
    /// so the Python slot is always freed within a few seconds.
    /// Returns false when the run is unknown or already finished.
    bool cancel(quint64 run_id);

    /// True while the run has neither completed, failed nor been cancelled.
    bool is_running(quint64 run_id) const { return runs_.contains(run_id); }

    /// Load the strategy catalogue of a provider — EVERY strategy, including the ones
    /// that cannot run here (each then carries `supported: false` and a reason).
    /// Cached 10 minutes. Arrives on result_ready(provider, "get_strategies", data).
    void load_strategies(const QString& provider);

    /// Load command option lists (label_types, splitter_types, etc.) for a provider
    void load_command_options(const QString& provider);

    /// get_capabilities of a provider (fill model, cost models, short selling, …);
    /// cached 24 h; capabilities_loaded() on success, nothing on failure (callers
    /// keep provider_support() as the fallback).
    void load_capabilities(const QString& provider);

    /// Store a portfolio config for BacktestingScreen to pick up on next show (or at
    /// once, via pending_portfolio_config_set, if a Backtesting panel is already visible).
    void set_pending_portfolio_config(const QJsonObject& config);
    /// Take (and clear) the pending config. Returns empty if none pending.
    QJsonObject take_pending_portfolio_config();

    /// Delay between the cooperative cancel flag and the kill of the process tree.
    static constexpr int kCancelGraceMs = 2000;

  signals:
    // ── Correlated (preferred) ──────────────────────────────────────────────
    void run_completed(quint64 run_id, QString provider, QString command, QJsonObject data);
    /// Emitted right before run_failed when the provider returned a structured
    /// error: its whole response ({error, errorCode, details{traceback, logs}} ...),
    /// so a consumer can show the provider log next to the message.
    void run_failed_details(quint64 run_id, QJsonObject response);
    void run_failed(quint64 run_id, QString context, QString message);
    void run_cancelled(quint64 run_id, QString provider, QString command);
    /// Every run that ends, except one the caller cancelled: the classified outcome
    /// (kind, errorCode, message, technical details, data). Emitted before the
    /// run_completed / run_failed / run_cancelled signal of the same run.
    void run_finished(quint64 run_id, const fincept::services::backtest::BtOutcome& outcome);
    /// A progress line of a running run (stderr "@@BT_PROGRESS {...}").
    void run_progress(quint64 run_id, const fincept::services::backtest::BtProgress& progress);

    // ── Legacy, uncorrelated (kept for existing consumers) ──────────────────
    void result_ready(QString provider, QString command, QJsonObject data);
    void command_options_loaded(QString provider, QJsonObject options);
    void capabilities_loaded(QString provider, QJsonObject capabilities);
    void error_occurred(QString context, QString message);

    /// A hand-off config was stored. A Backtesting panel that is already visible
    /// gets no show event, so it takes the config on this signal instead.
    void pending_portfolio_config_set();

  private:
    explicit BacktestingService(QObject* parent = nullptr);
    ~BacktestingService() override;
    Q_DISABLE_COPY(BacktestingService)

    struct Run {
        QString provider;
        QString command;
        std::shared_ptr<std::atomic<bool>> kill; ///< PythonRunner cancel token (kill fallback)
        qint64 started_ms = 0;
    };

    /// Run the provider script via PythonRunner (the pre-broker-injection dispatch).
    void dispatch_python(quint64 run_id, const QString& provider, const QString& command, QJsonObject args);
    /// GUI-thread half of a finished run: the outcome was classified on a worker.
    void finish_run(quint64 run_id, const QString& provider, const QString& command, const BtOutcome& outcome,
                    qint64 duration_ms);
    /// Path of the flag file cancel() creates for a run (polled by the provider).
    static QString cancel_flag_path(quint64 run_id);

    QHash<quint64, Run> runs_;
    QSet<quint64> cancelled_; // cancelled runs whose process has not reported back yet
    quint64 next_run_id_ = 1;
    QJsonObject pending_portfolio_config_;
};

} // namespace fincept::services::backtest

// Q_DECLARE_METATYPE(BtOutcome) lives beside the type in BacktestRunProtocol.h: declared
// here it came after the first use inside a unity batch (C2908).
