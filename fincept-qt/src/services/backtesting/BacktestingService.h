// src/services/backtesting/BacktestingService.h
#pragma once
#include "services/backtesting/BacktestingTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>

namespace fincept::services::backtest {

/// Singleton service dispatching backtesting commands to Python providers.
///
/// Every execute() call is a *run* with its own id. Consumers should correlate on
/// that id via run_completed / run_failed / run_cancelled: the legacy result_ready /
/// error_occurred signals carry no id, so two concurrent callers (Backtesting
/// screen, Portfolio, the workflow "Backtest Engine" node) cannot tell their
/// results apart. The legacy signals are still emitted (for every run that was not
/// cancelled) so existing consumers keep working.
class BacktestingService : public QObject {
    Q_OBJECT
  public:
    static BacktestingService& instance();

    /// Execute a backtesting command on a specific provider. Returns the run id
    /// (never 0) that run_completed / run_failed / run_cancelled will carry.
    quint64 execute(const QString& provider, const QString& command, const QJsonObject& args);

    /// Cancel an in-flight run. The run's eventual result is discarded (no
    /// result_ready / run_completed is emitted for it) and run_cancelled fires
    /// immediately. Long provider loops (optimize, walk-forward) also stop early:
    /// the provider polls a cancel-flag file between iterations and exits, which
    /// frees its PythonRunner slot. A single long library call cannot be
    /// interrupted (PythonRunner exposes no kill handle) — that process runs on
    /// until it finishes or its watchdog fires, but its output is ignored.
    /// Returns false when the run is unknown or already finished.
    bool cancel(quint64 run_id);

    /// True while the run has neither completed, failed nor been cancelled.
    bool is_running(quint64 run_id) const { return runs_.contains(run_id); }

    /// Load strategies dynamically from any provider
    void load_strategies(const QString& provider);

    /// Load command option lists (label_types, splitter_types, etc.) for a provider
    void load_command_options(const QString& provider);

    /// Store a portfolio config for BacktestingScreen to pick up on next show.
    void set_pending_portfolio_config(const QJsonObject& config);
    /// Take (and clear) the pending config. Returns empty if none pending.
    QJsonObject take_pending_portfolio_config();

  signals:
    // ── Correlated (preferred) ──────────────────────────────────────────────
    void run_completed(quint64 run_id, QString provider, QString command, QJsonObject data);
    void run_failed(quint64 run_id, QString context, QString message);
    void run_cancelled(quint64 run_id, QString provider, QString command);

    // ── Legacy, uncorrelated (kept for existing consumers) ──────────────────
    void result_ready(QString provider, QString command, QJsonObject data);
    void command_options_loaded(QString provider, QJsonObject options);
    void error_occurred(QString context, QString message);

  private:
    explicit BacktestingService(QObject* parent = nullptr);
    ~BacktestingService() override;
    Q_DISABLE_COPY(BacktestingService)

    struct Run {
        QString provider;
        QString command;
    };

    /// Run the provider script via PythonRunner (the pre-broker-injection dispatch).
    void dispatch_python(quint64 run_id, const QString& provider, const QString& command, QJsonObject args);
    /// Path of the flag file cancel() creates for a run (polled by the provider).
    static QString cancel_flag_path(quint64 run_id);

    QHash<quint64, Run> runs_;
    QSet<quint64> cancelled_; // cancelled runs whose process has not reported back yet
    quint64 next_run_id_ = 1;
    QJsonObject pending_portfolio_config_;
};

} // namespace fincept::services::backtest
