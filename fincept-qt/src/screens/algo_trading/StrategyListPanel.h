// src/screens/algo_trading/StrategyListPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QEvent>
#include <QString>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

class StrategyLibraryView;

/// MY STRATEGIES — the controller of StrategyLibraryView: loads the library
/// (AlgoTradingService), aggregates the paper / live deployments per strategy
/// (AlgoEngine), and runs the row actions: OPEN (edit_requested → the Builder),
/// BACKTEST (symbol + period prompt → backtest_requested), DEPLOY (PAPER) (the
/// deploy dialog; nothing starts until it is confirmed), DUPLICATE (a saved copy),
/// EXPORT (a JSON file) and DELETE (after a confirmation).
class StrategyListPanel : public QWidget {
    Q_OBJECT
  public:
    explicit StrategyListPanel(QWidget* parent = nullptr);

    /// Select a strategy's row (a deep link): at once when listed, else after the next load.
    /// Only selects — OPEN / BACKTEST / DEPLOY stay the user's.
    void select_strategy(const QString& strategy_id);

  signals:
    void edit_requested(fincept::services::algo::AlgoStrategy strategy);
    void backtest_requested(fincept::services::algo::AlgoStrategy strategy, QString symbol, QString start_date,
                            QString end_date);
    // Emitted after a deployment was started from this list (parent jumps to the Dashboard).
    void deployed();
    /// The "no strategies yet" state's OPEN BUILDER (the screen switches to the Builder).
    void open_builder_requested();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void connect_service();
    void on_strategies_loaded(const QVector<fincept::services::algo::AlgoStrategy>& strategies);
    void on_action(const QString& action, const QString& strategy_id);
    bool find_strategy(const QString& id, fincept::services::algo::AlgoStrategy* out) const;
    void prompt_backtest(const fincept::services::algo::AlgoStrategy& strategy);
    void deploy(const fincept::services::algo::AlgoStrategy& strategy);
    void duplicate(const fincept::services::algo::AlgoStrategy& strategy);
    void export_json(const fincept::services::algo::AlgoStrategy& strategy);
    void confirm_delete(const fincept::services::algo::AlgoStrategy& strategy);

    StrategyLibraryView* view_ = nullptr;
    QVector<fincept::services::algo::AlgoStrategy> strategies_; // full list
    QString select_after_load_; ///< a copy just saved: selected once the list reloads
    QString link_select_;       ///< a deep-linked strategy: selected once the list (re)loads
};

} // namespace fincept::screens
