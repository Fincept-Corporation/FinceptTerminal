// src/ui/widgets/algo/RiskManagementPanel.h
#pragma once
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLabel>
#include <QWidget>

namespace fincept::ui::algo {

/// Risk & sizing of a strategy as a compact wrap-around grid of numeric fields with units
/// (no sliders): STOP LOSS · TAKE PROFIT · TRAILING · ALLOC (backtest and deployment) and
/// QTY · MAX ORDER (deployment only — the backtest sizes by ALLOC; their tooltips say so).
/// 0 shows as "off" / "no limit". Every field explains itself on hover. Styled by object
/// name (#algoFieldLabel / #algoField / #algoNote of the Algo kit's root stylesheet).
class RiskManagementPanel : public QWidget {
    Q_OBJECT
  public:
    explicit RiskManagementPanel(QWidget* parent = nullptr);

    double stop_loss() const;
    double take_profit() const;
    double trailing_stop() const;
    double quantity() const;
    double max_order_value() const;
    double capital_pct() const; // % of capital allocated per backtest entry

    void set_values(double sl, double tp, double ts, double qty, double mov, double capital_pct = 100.0);
    /// Currency of the money fields (max order value): the instrument's quote currency,
    /// shown as the spin box's unit and kept in step with the format settings. Empty =
    /// unknown: no unit.
    void set_currency(const QString& ccy);
    /// "SL 5 % · TP 15 % · trail off · 100 % of capital" (for a collapsed summary).
    QString summary_text() const;

  signals:
    void values_changed();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    struct Field {
        QLabel* label = nullptr;
        QDoubleSpinBox* spin = nullptr;
    };

    Field create_field(double min_val, double max_val, double default_val, int decimals, double step,
                       const QString& suffix, QWidget* host);

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    Field stop_loss_;
    Field take_profit_;
    Field trailing_stop_;
    Field capital_pct_;
    Field quantity_;
    Field max_order_;
    QLabel* deploy_caption_ = nullptr;
};

} // namespace fincept::ui::algo
