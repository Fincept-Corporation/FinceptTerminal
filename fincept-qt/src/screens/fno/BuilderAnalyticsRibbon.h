#pragma once
// BuilderAnalyticsRibbon — top-of-Builder readout. Same KV pattern as
// FnoHeaderBar so the two ribbons feel of a piece visually:
//
//   PREMIUM 1240   MAX PROFIT  ∞   MAX LOSS −820   BREAKEVENS 24050 / 24330
//   POP 32.4%   Δ +0.42   Γ +0.018   Θ −18.3   V +95.2   MARGIN —
//
// Margin stays "—" until Phase 6 wires `IBroker::get_basket_margins`. Net
// premium is signed: positive = net debit (paid), negative = net credit.

#include "services/options/OptionChainTypes.h"

#include <QLabel>
#include <QString>
#include <QWidget>

namespace fincept::screens::fno {

class BuilderAnalyticsRibbon : public QWidget {
    Q_OBJECT
  public:
    explicit BuilderAnalyticsRibbon(QWidget* parent = nullptr);

    /// Populate every cell from analytics + premium.
    void update_from(const fincept::services::options::Strategy& s,
                     const fincept::services::options::StrategyAnalytics& a);

    /// Empty state — every cell shows "—".
    void clear();

    /// Push the basket-margin readout from the broker (or paper estimate).
    /// `value` is in account currency. `estimated=true` greys the cell to
    /// signal the number is a heuristic, not a broker response.
    void set_margin(double value, bool estimated = false);

    /// Reset the margin cell to "—" (e.g. while a fetch is in flight).
    void clear_margin();

  private:
    void setup_ui();

    QLabel* lbl_premium_ = nullptr;
    QLabel* lbl_max_profit_ = nullptr;
    QLabel* lbl_max_loss_ = nullptr;
    QLabel* lbl_breakevens_ = nullptr;
    QLabel* lbl_pop_ = nullptr;
    QLabel* lbl_delta_ = nullptr;
    QLabel* lbl_gamma_ = nullptr;
    QLabel* lbl_theta_ = nullptr;
    QLabel* lbl_vega_ = nullptr;
    QLabel* lbl_margin_ = nullptr;
};

} // namespace fincept::screens::fno
