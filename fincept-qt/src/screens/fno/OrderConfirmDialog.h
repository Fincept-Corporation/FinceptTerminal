#pragma once
// OrderConfirmDialog — modal preview shown before the F&O Builder dispatches
// a strategy as paper orders. Renders the leg table, computed premium /
// max P/L, and an estimated basket margin (fetched async from
// `IBroker::get_basket_margins` against the chain's broker).
//
// The dialog is purely presentational: it does NOT place orders. On accept,
// `BuilderSubTab` reads `legs()` + `basket_margin()` and dispatches via
// `pt_place_order`. This keeps the dialog reusable for live-trade preview
// in a later phase without coupling it to the paper engine.

#include "services/options/OptionChainTypes.h"
#include "trading/TradingTypes.h"

#include <QDialog>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QTableWidget>

#include <optional>

namespace fincept::screens::fno {

class OrderConfirmDialog : public QDialog {
    Q_OBJECT
  public:
    OrderConfirmDialog(const fincept::services::options::Strategy& strategy,
                       const fincept::services::options::OptionChain& chain,
                       double premium, double max_profit, double max_loss,
                       QWidget* parent = nullptr);

    /// Margin returned by the broker. Empty until the async fetch completes;
    /// the dialog can be accepted without it (the loader times out at 5s).
    std::optional<fincept::trading::BasketMargin> basket_margin() const { return margin_; }

    const fincept::services::options::Strategy& strategy() const { return strategy_; }

  private:
    void setup_ui(double premium, double max_profit, double max_loss);
    void populate_legs();
    void start_margin_fetch();
    void on_margin_loaded(bool ok, fincept::trading::BasketMargin margin, QString error);

    fincept::services::options::Strategy strategy_;
    fincept::services::options::OptionChain chain_;
    std::optional<fincept::trading::BasketMargin> margin_;

    QTableWidget* legs_table_ = nullptr;
    QLabel* lbl_premium_ = nullptr;
    QLabel* lbl_max_pnl_ = nullptr;
    QLabel* lbl_margin_ = nullptr;
    QPushButton* place_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
};

} // namespace fincept::screens::fno
