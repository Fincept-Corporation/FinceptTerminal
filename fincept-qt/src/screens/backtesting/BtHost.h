// src/screens/backtesting/BtHost.h
//
// What the Backtesting screen offers its analysis / research / history tabs (implemented by
// BacktestingScreen). Header-only interface, not a QObject.
#pragma once

#include "services/backtesting/BacktestRequest.h"
#include "services/backtesting/BacktestResultModel.h"
#include "services/backtesting/BacktestingTypes.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::screens::bt {

class BtRunController;

class BtHost {
  public:
    virtual ~BtHost() = default;

    /// The complete current setup (input strip + setup panel).
    virtual services::backtest::BtSetup current_setup() const = 0;
    /// Why the setup cannot run now (no symbol, bad dates, a parameter rule, an unsupported strategy);
    /// empty when it can.
    virtual QString setup_problem() const = 0;
    /// Parameter schema of the selected strategy.
    virtual QVector<services::backtest::StrategyParam> strategy_params() const = 0;
    virtual QString engine_slug() const = 0;
    virtual QString engine_label() const = 0;
    /// The engine offers `command` (backtest, optimize, walk_forward, monte_carlo, indicator, …).
    virtual bool engine_offers(const QString& command) const = 0;
    virtual BtRunController* controller() const = 0;
    /// The active backtest run (the ribbon's), or null.
    virtual services::backtest::BtRunPtr active_run() const = 0;
    /// Account currency of the current setup (first symbol's trading currency).
    virtual QString setup_currency() const = 0;
    /// APPLY an optimised parameter set to the setup.
    virtual void apply_params(const QJsonObject& params) = 0;
    /// Run the backtest of the setup with these parameters (becomes the active run).
    virtual void run_backtest_with(const QJsonObject& params) = 0;
    /// Make a saved run the active run (RUNS → OPEN, COMPARE → OPEN).
    virtual void open_saved_run(qint64 store_id) = 0;
    /// Load a saved run's inputs into the setup (and run it when `run`).
    virtual void load_saved_inputs(qint64 store_id, bool run) = 0;
    /// Show a transient message in the status bar.
    virtual void notify(const QString& text) = 0;
};

} // namespace fincept::screens::bt
