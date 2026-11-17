// src/ui/widgets/algo/ExecutionCostPanel.h
#pragma once
#include "algo_engine/ExecutionModel.h"

#include <QEvent>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QGridLayout;

namespace fincept::ui::algo {

/// Strategy-level execution settings shared by the backtest and paper fills:
/// entry side (long / short), the cost model (preset + editable commission,
/// taxes, slippage, lot size, borrow fee) and the risk-free rate used for
/// Sharpe / Sortino. Presets are labelled approximations; editing any field
/// switches the preset to "Custom".
class ExecutionCostPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ExecutionCostPanel(QWidget* parent = nullptr);

    fincept::algo::CostModel cost_model() const;
    void set_cost_model(const fincept::algo::CostModel& model);
    /// Apply a preset by id (keeps entry side and risk-free rate).
    void apply_preset(const QString& preset_id);
    /// Select the venue's approximate preset — only while the user has not chosen
    /// one (a fresh strategy). `venue_or_country` as for execution::default_preset_for.
    void suggest_for(const QString& venue_or_country, bool intraday);
    /// True once the user picked a preset or edited a field.
    bool user_chose() const { return user_chose_; }
    void set_user_chose(bool on) { user_chose_ = on; }

    QString entry_side() const; ///< "BUY" | "SELL"
    void set_entry_side(const QString& side);
    double risk_free_pct() const;
    void set_risk_free_pct(double pct);
    /// Currency of the flat commission fields (per unit, per order, minimum, maximum):
    /// the instrument's quote currency, shown as their unit and kept in step with the
    /// format settings. Empty = unknown: no unit.
    void set_currency(const QString& ccy);
    /// One line for a collapsed section: "long entries · US shares · commission-free · 2 bps
    /// slippage · signals fill at the next bar's open".
    QString summary_text() const;
    /// Hands the entry-side combo to a host that shows it elsewhere (e.g. its input strip): the
    /// panel hides its own "Entry side" caption; its layout lets go of the re-parented combo.
    QComboBox* take_side_combo();

  signals:
    void changed();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    QDoubleSpinBox* make_spin(double max, int decimals, double step, const QString& suffix);
    void fill_presets();
    void on_preset_activated(int index);
    void on_field_edited();
    void update_note();
    void retranslateUi();

    QLabel* header_ = nullptr;
    QLabel* side_label_ = nullptr;
    QComboBox* side_combo_ = nullptr;
    QLabel* preset_label_ = nullptr;
    QComboBox* preset_combo_ = nullptr;
    QLabel* note_ = nullptr;

    QLabel* comm_pct_label_ = nullptr;
    QDoubleSpinBox* comm_pct_ = nullptr;
    QLabel* comm_unit_label_ = nullptr;
    QDoubleSpinBox* comm_unit_ = nullptr;
    QLabel* comm_order_label_ = nullptr;
    QDoubleSpinBox* comm_order_ = nullptr;
    QLabel* comm_min_label_ = nullptr;
    QDoubleSpinBox* comm_min_ = nullptr;
    QLabel* comm_max_label_ = nullptr;
    QDoubleSpinBox* comm_max_ = nullptr;
    QLabel* buy_tax_label_ = nullptr;
    QDoubleSpinBox* buy_tax_ = nullptr;
    QLabel* sell_tax_label_ = nullptr;
    QDoubleSpinBox* sell_tax_ = nullptr;
    QLabel* slip_bps_label_ = nullptr;
    QDoubleSpinBox* slip_bps_ = nullptr;
    QLabel* slip_atr_label_ = nullptr;
    QDoubleSpinBox* slip_atr_ = nullptr;
    QLabel* lot_label_ = nullptr;
    QDoubleSpinBox* lot_ = nullptr;
    QLabel* borrow_label_ = nullptr;
    QDoubleSpinBox* borrow_ = nullptr;
    QLabel* rf_label_ = nullptr;
    QDoubleSpinBox* rf_ = nullptr;

    QString preset_id_ = QStringLiteral("none");
    bool user_chose_ = false;
    bool loading_ = false; // programmatic updates do not mark the model custom
};

} // namespace fincept::ui::algo
