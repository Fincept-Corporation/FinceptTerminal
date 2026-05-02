#pragma once

#include <QWidget>

class QFrame;
class QLabel;

namespace fincept::screens::panels {
class SwapPanel;
class FeeDiscountPanel;
}

namespace fincept::screens {

/// TRADE tab — buy/sell $FNCPT via PumpPortal, plus fee-discount eligibility.
///
/// SwapPanel handles BUY/SELL through `PumpFunSwapService::build_swap` →
/// `WalletService::sign_and_send`. FeeDiscountPanel surfaces the 30 % discount
/// chip when the wallet holds ≥ 1,000 $FNCPT.
///
/// Burn was considered for Stage 2.3 and **deferred to Phase 5** (see
/// `plans/crypto-center-phase-2.md` D6). The buyback worker burns from the
/// treasury; per-user burn is no longer in scope here.
class TradeTab : public QWidget {
    Q_OBJECT
  public:
    explicit TradeTab(QWidget* parent = nullptr);
    ~TradeTab() override;

    /// Pre-fill SwapPanel's FROM combo with the given mint. Wired by
    /// CryptoCenterScreen to HomeTab::select_token so clicking a row in the
    /// holdings table on HOME drops the user into TRADE with that asset
    /// pre-selected.
    void set_from_mint(const QString& mint);

  private:
    void build_ui();
    void apply_theme();

    panels::SwapPanel* swap_panel_ = nullptr;
    panels::FeeDiscountPanel* fee_discount_panel_ = nullptr;
    QFrame* fee_discount_placeholder_ = nullptr;
};

} // namespace fincept::screens
