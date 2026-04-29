#pragma once

#include <QWidget>

class QFrame;
class QLabel;

namespace fincept::screens::panels {
class SwapPanel;
class FeeDiscountPanel;
}

namespace fincept::screens {

/// TRADE tab — buy/sell $FNCPT via Jupiter and burn $FNCPT.
///
/// Stage 2.2 ships SwapPanel; placeholder slots reserved for BurnPanel
/// (Stage 2.3) and FeeDiscountPanel (Stage 2.4). When those land they take
/// over the placeholder QFrames keyed by `objectName`.
class TradeTab : public QWidget {
    Q_OBJECT
  public:
    explicit TradeTab(QWidget* parent = nullptr);
    ~TradeTab() override;

  private:
    void build_ui();
    void apply_theme();

    panels::SwapPanel* swap_panel_ = nullptr;
    panels::FeeDiscountPanel* fee_discount_panel_ = nullptr;

    // Burn placeholder — burn deferred to Phase 5 (see plans/crypto-center-phase-2.md D6).
    QFrame* burn_placeholder_ = nullptr;
    QFrame* fee_discount_placeholder_ = nullptr;
};

} // namespace fincept::screens
