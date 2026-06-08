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
#include "services/options/OptionChainTypes.h"
#include "services/options/StrategyTemplates.h"

#include <QEvent>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QPushButton;
class QSpinBox;
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

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

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

    qint64 loaded_strategy_id_ = 0;
    QString loaded_strategy_name_;
};

} // namespace fincept::screens::fno
