#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>

namespace fincept::screens {

class AlgoDeployDialog : public QDialog {
    Q_OBJECT
  public:
    explicit AlgoDeployDialog(const QString& strategy_id, const QString& strategy_name, QWidget* parent = nullptr);
    explicit AlgoDeployDialog(const fincept::services::algo::AlgoStrategy& strategy, QWidget* parent = nullptr);

    fincept::services::algo::AlgoDeployment result() const;
    fincept::services::algo::AlgoDeployment deployment() const { return result_; }

  private slots:
    void on_mode_changed(int index);
    void on_account_changed(int index);
    void on_ok();

  private:
    void build_ui();
    void populate_accounts();
    void populate_broker_fields(const QString& account_id);

    QString strategy_id_;
    QString strategy_name_;

    QLineEdit*      symbol_edit_        = nullptr;
    QComboBox*      mode_combo_         = nullptr;
    QComboBox*      side_combo_         = nullptr;
    QComboBox*      account_combo_      = nullptr;
    QComboBox*      exchange_combo_     = nullptr;
    QComboBox*      product_type_combo_ = nullptr;
    QComboBox*      timeframe_combo_    = nullptr;
    QDoubleSpinBox* quantity_spin_      = nullptr;
    QDoubleSpinBox* max_order_spin_     = nullptr;
    QDoubleSpinBox* max_loss_spin_      = nullptr;

    QLabel*   account_label_      = nullptr;
    QLabel*   exchange_label_     = nullptr;
    QLabel*   product_type_label_ = nullptr;

    fincept::services::algo::AlgoDeployment result_;
};

} // namespace fincept::screens
