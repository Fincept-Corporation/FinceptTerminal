#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>

class QDialogButtonBox;

namespace fincept::screens {

class AlgoDeployDialog : public QDialog {
    Q_OBJECT
  public:
    explicit AlgoDeployDialog(const QString& strategy_id, const QString& strategy_name, QWidget* parent = nullptr);
    explicit AlgoDeployDialog(const fincept::services::algo::AlgoStrategy& strategy, QWidget* parent = nullptr);

    fincept::services::algo::AlgoDeployment result() const;
    fincept::services::algo::AlgoDeployment deployment() const { return result_; }

    // Prefill the symbol field (the builder's selected symbol isn't part of
    // AlgoStrategy, so it's pushed in after construction).
    void set_symbol(const QString& symbol);

    // F&O context for option/future deployments (set by the StrategyBuilder before exec()).
    void set_fno_context(const QString& instrument_type, const QString& underlying, const QString& expiry_rule);

    // Seed the risk fields from the caller's risk panel BEFORE exec(), so the
    // numbers the user confirms in this dialog are the numbers that are actually
    // deployed. Callers must not overwrite result().quantity afterwards — that
    // made the dialog lie about the size going live.
    void set_risk_defaults(double quantity, double max_order_value);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_mode_changed(int index);
    void on_account_changed(int index);
    void on_ok();

  private:
    void build_ui();
    void populate_accounts();
    void populate_broker_fields(const QString& account_id);
    void retranslateUi();

    QString strategy_id_;
    QString strategy_name_;

    QString fno_instrument_type_ = "equity";
    QString fno_underlying_;
    QString fno_expiry_rule_;

    QLineEdit* symbol_edit_ = nullptr;
    QComboBox* mode_combo_ = nullptr;
    QComboBox* side_combo_ = nullptr;
    QComboBox* account_combo_ = nullptr;
    QComboBox* exchange_combo_ = nullptr;
    QComboBox* product_type_combo_ = nullptr;
    QComboBox* timeframe_combo_ = nullptr;
    QDoubleSpinBox* quantity_spin_ = nullptr;
    QDoubleSpinBox* max_order_spin_ = nullptr;
    QDoubleSpinBox* max_loss_spin_ = nullptr;

    QLabel* title_label_ = nullptr;
    // Persistent red banner shown only while Mode == Live. Real money is the
    // one state that must never be inferred from a dropdown alone.
    QLabel* live_warning_ = nullptr;
    QLabel* symbol_label_ = nullptr;
    QLabel* mode_label_ = nullptr;
    QLabel* side_label_ = nullptr;
    QLabel* account_label_ = nullptr;
    QLabel* exchange_label_ = nullptr;
    QLabel* product_type_label_ = nullptr;
    QLabel* timeframe_label_ = nullptr;
    QLabel* quantity_label_ = nullptr;
    QLabel* max_order_label_ = nullptr;
    QLabel* max_loss_label_ = nullptr;
    QDialogButtonBox* buttons_ = nullptr;

    fincept::services::algo::AlgoDeployment result_;
};

} // namespace fincept::screens
