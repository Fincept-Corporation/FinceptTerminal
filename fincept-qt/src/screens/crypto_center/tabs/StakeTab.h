#pragma once

#include <QWidget>

namespace fincept::screens::panels {
class LockPanel;
class ActiveLocksPanel;
class TierPanel;
}

namespace fincept::screens {

/// STAKE tab — Phase 3 veFNCPT lock + tier system.
///
/// Stack: LockPanel (top) → ActiveLocksPanel (middle) → TierPanel (bottom).
/// Wrapped in a QScrollArea per Phase 1.5 U4 so short windows don't push
/// the tier card off-screen.
///
/// Replaces the Phase 1 ComingSoonTab placeholder. Each panel owns its own
/// hub-subscription lifecycle (showEvent/hideEvent) — the tab itself
/// subscribes nothing.
class StakeTab : public QWidget {
    Q_OBJECT
  public:
    explicit StakeTab(QWidget* parent = nullptr);
    ~StakeTab() override;

  private:
    void build_ui();
    void apply_theme();

    panels::LockPanel*        lock_panel_   = nullptr;
    panels::ActiveLocksPanel* active_locks_ = nullptr;
    panels::TierPanel*        tier_panel_   = nullptr;
};

} // namespace fincept::screens
