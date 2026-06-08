#pragma once
// OptionChainTable — QTableView subclass for the F&O Chain sub-tab.
//
// Wraps a QTableView with a custom delegate that draws horizontal OI bars
// behind the OI columns (CE OI bar grows leftward, PE OI bar grows
// rightward) plus ATM strike highlighting and ITM/OTM background tinting.

#include <QStyledItemDelegate>
#include <QTableView>

namespace fincept::screens::fno {

class OptionChainModel;

class OptionChainTable : public QTableView {
    Q_OBJECT
  public:
    explicit OptionChainTable(QWidget* parent = nullptr);

    OptionChainModel* chain_model() const { return model_; }

  signals:
    /// Emitted when the user left-clicks a CE or PE LTP cell (or picks
    /// "Add to Builder") — used by the strategy builder (Phase 5) to add a
    /// leg. `is_call=true` means CE side. `lots` is +1 (buy).
    void leg_clicked(qint64 token, double strike, bool is_call, int lots);

    /// Emitted when the user picks Buy/Sell from a leg's right-click menu.
    /// `is_buy=false` means Sell. ChainSubTab routes it to the broker (live
    /// or paper, per the connected account's trading mode).
    void order_requested(qint64 token, double strike, bool is_call, bool is_buy);

  protected:
    void mousePressEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

  private:
    OptionChainModel* model_ = nullptr;
};

} // namespace fincept::screens::fno
