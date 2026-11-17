#pragma once
// BuilderSubTab -- F&O Builder content widget (Bloomberg-density revamp).
//
// Layout (vertical, no splitters)
// ────────────────────────────────
//   BuilderAnalyticsRibbon  (2 rows, 44px)
//   TemplateToolbar         (28px)
//   LegEditorTable          (stretch 1)
//   PayoffStripWidget       (fixed 80px)
//   Footer row              (save / load / target / PCR / OI / trade)

#include "screens/common/IStatefulScreen.h"
#include "screens/fno/FnoChainState.h"
#include "services/options/OptionChainTypes.h"
#include "services/options/StrategyTemplates.h"

#include <QEvent>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QToolButton;

namespace fincept::screens::fno {

class BuilderAnalyticsRibbon;
class LegEditorTable;
class PayoffStripWidget;
class TemplateToolbar;

class BuilderSubTab : public QWidget {
    Q_OBJECT
  public:
    explicit BuilderSubTab(QWidget* parent = nullptr);
    ~BuilderSubTab() override;

    LegEditorTable* legs_view() const { return legs_view_; }

    /// Legs, strategy name, loaded id and target days — the Builder reopens on them.
    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

    /// A leg picked on the Chain tab. It replaces the opening ATM straddle the tab
    /// put there by itself (untouched), else it is appended.
    void append_chain_leg(const fincept::services::options::StrategyLeg& leg);

    /// The Chain tab's state (FnoScreen relays it): skeleton / "needs an option
    /// chain" page while this tab holds no chain.
    void set_chain_status(FnoChainStatus status, const QString& reason);
    /// A chain that does not travel on DataHub (the free delayed US chain), or an
    /// empty chain when the Chain tab left it.
    void set_external_chain(const fincept::services::options::OptionChain& chain);

  signals:
    /// OPEN CHAIN on this tab's page.
    void open_chain_requested();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_template_chosen(const QString& template_id,
                            const fincept::services::options::StrategyInstantiationOptions& options);
    void on_legs_changed();
    void on_save_clicked();
    void on_load_clicked();
    void on_trade_clicked();

  private:
    void setup_ui();
    void retranslateUi();

    void on_chain_published(const QString& topic, const QVariant& v);
    /// Take `chain` (already accepted as the Chain tab's series) and re-price.
    void apply_chain(const fincept::services::options::OptionChain& chain);
    /// With no legs and a chain on screen: the ATM long straddle, once per series
    /// (class A — local math); re-built for a new series while it is untouched.
    void seed_default_strategy();
    /// Legs / payoff, skeleton or the no-chain page.
    void update_body();
    /// Rate the payoff and POP are priced with: the chain's market (US or India).
    double rate_for_chain() const;
    /// Footer PCR / OI readouts and the ribbon's currency from last_chain_ (a new
    /// chain, or a currency / number-format change — nothing is fetched again).
    void render_chain_footer();
    void refresh_analytics();
    fincept::services::options::Strategy current_strategy() const;
    void update_trade_button_state();

    BuilderAnalyticsRibbon* ribbon_ = nullptr;
    TemplateToolbar* toolbar_ = nullptr;
    LegEditorTable* legs_view_ = nullptr;
    PayoffStripWidget* chart_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QToolButton* load_btn_ = nullptr;
    QPushButton* trade_btn_ = nullptr;
    QSpinBox* days_to_target_spin_ = nullptr;
    QLabel* pcr_label_ = nullptr;
    QLabel* ce_oi_label_ = nullptr;
    QLabel* pe_oi_label_ = nullptr;

    // Footer field labels (cached for retranslateUi)
    QLabel* target_label_ = nullptr;
    QLabel* pcr_key_ = nullptr;
    QLabel* ce_key_ = nullptr;
    QLabel* pe_key_ = nullptr;

    fincept::services::options::OptionChain last_chain_;
    bool chain_subscribed_ = false;

    /// P3: chain publishes keep arriving while the Builder tab is hidden (the
    /// service signal is connected for the widget's lifetime). Cache the
    /// snapshot but defer the payoff/POP/Greeks recompute until the tab is
    /// actually visible — compute_pop alone samples 1001 points per publish.
    bool is_visible_ = false;
    bool analytics_dirty_ = false;

    qint64 loaded_strategy_id_ = 0;
    QString loaded_strategy_name_;

    // States: body_ = work_ (legs + payoff) / skeleton_ / no_chain_.
    QStackedWidget* body_ = nullptr;
    QWidget* work_ = nullptr;
    fincept::ui::LoadingSkeleton* skeleton_ = nullptr;
    fincept::ui::EmptyState* no_chain_ = nullptr;
    FnoChainStatus chain_status_ = FnoChainStatus::Loading;
    QString chain_reason_;
    /// The legs are the opening ATM straddle the tab added itself and nobody has
    /// edited them since (a chain click replaces them; a new series re-builds them).
    bool default_legs_ = false;
    bool applying_default_ = false;
    QString default_series_;      ///< series the default legs were built for
    QSet<QString> default_tried_; ///< series that already got their default once
};

} // namespace fincept::screens::fno
