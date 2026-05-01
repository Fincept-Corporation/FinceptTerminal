#pragma once

#include <QWidget>

namespace fincept::screens::panels {
class BuybackBurnPanel;
class SupplyChartPanel;
class TreasuryPanel;
}

namespace fincept::screens {

/// ROADMAP tab — Phase 5 buyback & burn dashboard.
///
/// Stack: BuybackBurnPanel (top) → SupplyChartPanel (middle) →
/// TreasuryPanel (bottom). Wrapped in a QScrollArea per P9 + Phase 1.5 U4
/// so short windows don't push the treasury card off-screen.
///
/// Replaces the Phase 1 ComingSoonTab placeholder. Subscribes nothing
/// itself; each panel owns its own lifecycle (subscribe in showEvent,
/// unsubscribe in hideEvent).
class RoadmapTab : public QWidget {
    Q_OBJECT
  public:
    explicit RoadmapTab(QWidget* parent = nullptr);
    ~RoadmapTab() override;

  private:
    void build_ui();
    void apply_theme();

    panels::BuybackBurnPanel* buyback_burn_ = nullptr;
    panels::SupplyChartPanel* supply_chart_ = nullptr;
    panels::TreasuryPanel*    treasury_     = nullptr;
};

} // namespace fincept::screens
