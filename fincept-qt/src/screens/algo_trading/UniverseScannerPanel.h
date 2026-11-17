// src/screens/algo_trading/UniverseScannerPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QString>
#include <QVector>
#include <QWidget>

namespace fincept::ui {
class InputMemory;
}

namespace fincept::screens {

class UniverseView;

/// Universe tab — run a saved strategy live across all equities of an exchange the
/// selected broker trades (from its instrument master), one of your saved symbol
/// lists, or a custom list. Alerts on every match; each match has a DEPLOY action
/// that opens the existing single-symbol AlgoDeployDialog. The controller of
/// UniverseView (strategies, accounts, universes, the ScanMonitor watch, the
/// designed states, remembered inputs).
class UniverseScannerPanel : public QWidget {
    Q_OBJECT
  public:
    explicit UniverseScannerPanel(QWidget* parent = nullptr);

  signals:
    /// "BUILD A STRATEGY" on the no-strategy state (the screen opens the Builder).
    void open_builder_requested();

  protected:
    // Re-reads the broker accounts so one connected after this tab was built shows up.
    void showEvent(QShowEvent* event) override;

  private:
    void on_strategies_loaded(const QVector<fincept::services::algo::AlgoStrategy>& strategies);
    void on_start_stop();
    void on_match(const QString& watch_id, const QString& symbol, double price, const QString& detail);
    void on_universe_changed(int index);
    void populate_accounts();
    /// Universe choices for the selected account: "All <exchange> equity" for each
    /// cash exchange its broker lists, your saved lists, NIFTY top 20, custom.
    void populate_universes();
    void deploy_symbol(const QString& symbol);
    bool current_strategy(fincept::services::algo::AlgoStrategy* out) const;
    /// The strategy pane (what the scan runs).
    void update_strategy_summary();
    /// The LIVE MATCHES area while it has no rows: a designed state — broker needed (OPEN
    /// ACCOUNTS / RETRY), no equity strategy, what START SCAN will run (a preview: the scan is
    /// a persistent watch and never starts by itself), or "scanning, no match yet".
    void update_matches_state();
    void on_matches_state_action();
    void set_running(bool running);

    UniverseView* view_ = nullptr;
    /// What the state's primary button does: "setup", "retry", "builder", "start", "" = nothing.
    QString state_action_;
    QString setup_screen_, setup_section_, setup_focus_; ///< where "setup" opens
    fincept::ui::InputMemory* inputs_ = nullptr; ///< strategy / universe / account / sweep / cooldown

    QVector<fincept::services::algo::AlgoStrategy> strategies_;
    QString watch_id_; // non-empty while a scan is running
    bool running_ = false;
    // The running scan's exchange (from the universe; empty = per symbol) and broker:
    // a match's price is shown in its listing's currency.
    QString scan_exchange_hint_;
    QString scan_broker_id_;
};

} // namespace fincept::screens
