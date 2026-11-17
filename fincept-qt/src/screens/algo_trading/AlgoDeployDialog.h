#pragma once
#include "algo_engine/fno/FnoAlgoTypes.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>

class QCheckBox;
class QDialogButtonBox;
class QSpinBox;
class QTimeEdit;

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
    // `legs` are the strategy's leg rules: an F&O deployment trades THEM (each leg's
    // side and lots × lot size), so the dialog hides Entry Side / Quantity and the
    // LIVE confirmation lists the legs instead.
    void set_fno_context(const QString& instrument_type, const QString& underlying, const QString& expiry_rule,
                         const QVector<fincept::algo::fno::AlgoFnoLeg>& legs = {});

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
    // One-line summary of the strategy's last backtest (or that there is none).
    QString backtest_summary_text() const;
    void refresh_backtest_note();
    // Risk limits as entered (more conservative extras; see algo_engine/RiskLimits.h).
    QJsonObject risk_limits_json() const;
    QString risk_limits_text() const;
    // Currency of the money limits (max order value, max daily loss, max drawdown):
    // the deployment's quote currency from the symbol / underlying, exchange and the
    // account's broker; empty when unknown (no unit shown, never a guessed one).
    QString deploy_currency() const;
    // Puts deploy_currency()'s unit on the money spin boxes (symbol, exchange, account changes).
    void refresh_money_units();

    QString strategy_id_;
    QString strategy_name_;
    QJsonObject last_backtest_; // strategy's persisted last-backtest summary
    QJsonObject cost_model_;    // strategy's cost model (copied into the deployment)
    QString backtested_side_ = QStringLiteral("BUY");

    QString fno_instrument_type_ = "equity";
    QString fno_underlying_;
    QString fno_expiry_rule_;
    QVector<fincept::algo::fno::AlgoFnoLeg> fno_legs_;

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
    QDoubleSpinBox* max_dd_spin_ = nullptr;
    QSpinBox* max_trades_spin_ = nullptr;
    QCheckBox* window_check_ = nullptr;
    QTimeEdit* window_start_ = nullptr;
    QTimeEdit* window_end_ = nullptr;
    QCheckBox* session_only_check_ = nullptr;
    QCheckBox* flatten_check_ = nullptr;
    QLabel* max_dd_label_ = nullptr;
    QLabel* max_trades_label_ = nullptr;
    QLabel* window_label_ = nullptr;
    QLabel* backtest_note_ = nullptr;
    QLabel* side_note_ = nullptr;

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
