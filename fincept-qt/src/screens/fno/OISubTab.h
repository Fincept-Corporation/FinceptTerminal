#pragma once
// OISubTab — F&O OI Analytics content widget. Replaces the placeholder in
// FnoScreen for SubTab::OI.
//
// Layout (2 × 2 splitter)
// ───────────────────────
//   ┌────────────────────────┬──────────────────────────────┐
//   │ MultiStrikeOIChart     │  MaxPainChart                │
//   ├────────────────────────┼──────────────────────────────┤
//   │ OIBuildupTable         │  [Strike ▾] IntradayOIChart  │
//   └────────────────────────┴──────────────────────────────┘
//
// Subscribes to `option:chain:*` (pattern) — pushes the latest publish into
// the three chain-derived widgets and re-seeds the IntradayOIChart's strike
// combo if the underlying changed. Visibility-driven sub/unsub per D3/D4.

#include "screens/common/IStatefulScreen.h"
#include "screens/fno/FnoChainState.h"
#include "services/options/OptionChainTypes.h"

#include <QEvent>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class QComboBox;
class QLabel;
class QStackedWidget;

namespace fincept::screens::fno {

class IntradayOIChart;
class MaxPainChart;
class MultiStrikeOIChart;
class OIBuildupTable;

class OISubTab : public QWidget {
    Q_OBJECT
  public:
    explicit OISubTab(QWidget* parent = nullptr);
    ~OISubTab() override;

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

    /// The Chain tab's state (FnoScreen relays it): drives the skeleton / the "needs
    /// an option chain" page while this tab holds no chain.
    void set_chain_status(FnoChainStatus status, const QString& reason);
    /// A chain that does not travel on DataHub (the free delayed US chain), or an
    /// empty chain when the Chain tab left it.
    void set_external_chain(const fincept::services::options::OptionChain& chain);

  signals:
    /// OPEN CHAIN on this tab's pages.
    void open_chain_requested();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_strike_combo_changed(int index);

  private:
    void setup_ui();
    void retranslateUi();
    void on_chain_published(const QString& topic, const QVariant& v);
    /// Render `chain` (already accepted as the Chain tab's series).
    void apply_chain(const fincept::services::options::OptionChain& chain);
    /// Content, skeleton or the no-chain page, from last_chain_ and the chain status.
    void update_body();
    void rebuild_strike_combo(const fincept::services::options::OptionChain& chain);
    void apply_strike_subscription();

    MultiStrikeOIChart* oi_bars_ = nullptr;
    MaxPainChart* pain_chart_ = nullptr;
    OIBuildupTable* buildup_ = nullptr;
    IntradayOIChart* intraday_ = nullptr;
    QComboBox* strike_combo_ = nullptr;
    QLabel* picker_lbl_ = nullptr;
    QStackedWidget* body_ = nullptr; ///< content / skeleton / no-chain page
    QWidget* content_ = nullptr;
    fincept::ui::LoadingSkeleton* skeleton_ = nullptr;
    fincept::ui::EmptyState* no_chain_ = nullptr;
    FnoChainStatus chain_status_ = FnoChainStatus::Loading;
    QString chain_reason_;
    /// What the intraday page's primary button does (its requirement status).
    fincept::ui::RequirementStatus intraday_req_;

    fincept::services::options::OptionChain last_chain_;
    QString current_underlying_;
    /// Expiry the strike combo was last built for. A new expiry has a different
    /// strike ladder, so the combo must be rebuilt even when the underlying is
    /// unchanged.
    QString current_expiry_;
    /// Strike the user picked (0 = none). Carried across combo rebuilds and
    /// session restore. Selection is keyed on the strike VALUE, never on a row
    /// index — the ladder shifts as the ATM moves.
    double selected_strike_ = 0.0;
    bool subscribed_ = false;
};

} // namespace fincept::screens::fno
