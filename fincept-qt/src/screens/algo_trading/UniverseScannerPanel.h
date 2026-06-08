// src/screens/algo_trading/UniverseScannerPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"
#include "ui/widgets/algo/SymbolChipInput.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTableWidget>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// Universe tab — run a saved strategy live across all NSE/BSE equities (or NIFTY 50
/// / a custom list). Alerts on every match; each match row has a Deploy button that
/// opens the existing single-symbol AlgoDeployDialog.
class UniverseScannerPanel : public QWidget {
    Q_OBJECT
  public:
    explicit UniverseScannerPanel(QWidget* parent = nullptr);

  private slots:
    void on_strategies_loaded(QVector<fincept::services::algo::AlgoStrategy> strategies);
    void on_start_stop();
    void on_match(const QString& watch_id, const QString& symbol, double price,
                  const QString& detail);
    void on_universe_changed(int index);

  private:
    void build_ui();
    void set_status(const QString& text, const QString& color);
    void deploy_symbol(const QString& symbol);
    bool current_strategy(fincept::services::algo::AlgoStrategy* out) const;

    QComboBox*    strategy_combo_  = nullptr;
    QComboBox*    universe_combo_  = nullptr;
    fincept::ui::algo::SymbolChipInput* symbols_input_ = nullptr;
    QLabel*       symbols_label_   = nullptr;
    QComboBox*    account_combo_   = nullptr;
    QSpinBox*     sweep_spin_      = nullptr;
    QSpinBox*     cooldown_spin_   = nullptr;
    QPushButton*  start_btn_       = nullptr;
    QTableWidget* matches_table_   = nullptr;
    QLabel*       status_label_    = nullptr;

    QVector<fincept::services::algo::AlgoStrategy> strategies_;
    QString watch_id_;     // non-empty while a scan is running
    bool    running_ = false;
};

} // namespace fincept::screens
