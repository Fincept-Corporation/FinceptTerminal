#pragma once
// BuilderSubTab — F&O Builder content widget. Replaces the placeholder in
// FnoScreen for SubTab::Builder.
//
// Layout
// ──────
//   ┌──────────────────────────────────────────────────────────────┐
//   │ BuilderAnalyticsRibbon                                       │
//   ├─────────────────────────┬────────────────────────────────────┤
//   │ TemplatePickerPanel     │                                    │
//   ├─────────────────────────┤        PayoffChartWidget           │
//   │ LegEditorTable          │                                    │
//   ├─────────────────────────┴────────────────────────────────────┤
//   │ [Save] [Load ▾]                          [Trade All — P6]    │
//   └──────────────────────────────────────────────────────────────┘
//
// Lifecycle
// ─────────
// On showEvent: subscribes to `option:chain:*` (pattern). The most recent
// chain publish populates `last_chain_` — the basis for both template
// instantiation and live combined-Greeks lookup. On hideEvent: unsubscribe.
// Pure D3/D4 compliance — no timers of our own; analytics recompute is
// driven by chain publish callbacks + leg-edit signals.

#include "screens/IStatefulScreen.h"
#include "services/options/OptionChainTypes.h"
#include "services/options/StrategyTemplates.h"

#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class QPushButton;
class QSpinBox;
class QToolButton;

namespace fincept::screens::fno {

class BuilderAnalyticsRibbon;
class LegEditorTable;
class PayoffChartWidget;
class TemplatePickerPanel;

class BuilderSubTab : public QWidget {
    Q_OBJECT
  public:
    explicit BuilderSubTab(QWidget* parent = nullptr);
    ~BuilderSubTab() override;

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_template_chosen(const QString& template_id,
                            const fincept::services::options::StrategyInstantiationOptions& options);
    void on_legs_changed();
    void on_save_clicked();
    void on_load_clicked();
    void on_trade_clicked();

  private:
    void setup_ui();

    /// Hub subscription handler — runs on the main thread.
    void on_chain_published(const QString& topic, const QVariant& v);

    /// Recompute analytics + redraw payoff curve from current legs +
    /// `last_chain_`. No-op when there are no legs or no chain.
    void refresh_analytics();

    /// Build a `Strategy` snapshot from the leg editor model + the chain's
    /// underlying / expiry.
    fincept::services::options::Strategy current_strategy() const;

    /// Resolve (creating if needed) the paper portfolio used for F&O paper
    /// dispatch. INR-denominated, NFO-scoped, leverage 1.0. Returns the id.
    static QString ensure_paper_portfolio();

    /// Toggle Trade All button enable based on leg count + chain readiness.
    void update_trade_button_state();

    BuilderAnalyticsRibbon* ribbon_ = nullptr;
    TemplatePickerPanel* picker_ = nullptr;
    LegEditorTable* legs_view_ = nullptr;
    PayoffChartWidget* chart_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QToolButton* load_btn_ = nullptr;
    QPushButton* trade_btn_ = nullptr;
    QSpinBox* days_to_target_spin_ = nullptr;

    /// Most recent chain seen on the hub. Empty until a publish arrives.
    fincept::services::options::OptionChain last_chain_;
    bool chain_subscribed_ = false;

    /// Row id of the saved strategy currently being edited; 0 = unsaved.
    /// Save uses this to choose between INSERT (new) and UPDATE (existing).
    qint64 loaded_strategy_id_ = 0;
    /// Display name for the currently-loaded strategy. Shown in the footer.
    QString loaded_strategy_name_;
};

} // namespace fincept::screens::fno
