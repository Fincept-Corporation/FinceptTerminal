#pragma once

#include <QWidget>

class QHideEvent;
class QShowEvent;

namespace fincept::screens {

namespace panels {
class MarketsListPanel;
}

/// MARKETS tab — internal prediction-market entry point (Phase 4 §4.2).
///
/// Wraps `MarketsListPanel` (curated/live market list, YES/NO prices, status
/// pill) inside the standard Crypto Center panel chrome. The tab itself owns
/// no data subscriptions — those live in the inner panel and respect P3
/// (visibility-driven lifecycle).
///
/// Phase 4 ships in **demo mode** until the Fincept matching engine and the
/// `fincept_market` Anchor program are deployed. The
/// `FinceptInternalAdapter` returns a curated three-market dataset; the
/// status pill in the panel head reads "DEMO" to make the state explicit.
class MarketsTab : public QWidget {
    Q_OBJECT
  public:
    explicit MarketsTab(QWidget* parent = nullptr);
    ~MarketsTab() override;

  private:
    void build_ui();
    void apply_theme();

    panels::MarketsListPanel* list_panel_ = nullptr;
};

} // namespace fincept::screens
