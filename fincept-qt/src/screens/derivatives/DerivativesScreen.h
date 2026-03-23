#pragma once

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens {

/// Professional derivatives pricing screen.
/// Supports: Bonds, Equity Options, FX Options, IR Swaps, Credit (CDS).
/// Calculations run via PythonRunner (derivatives_pricing.py).
class DerivativesScreen : public QWidget {
    Q_OBJECT
  public:
    explicit DerivativesScreen(QWidget* parent = nullptr);

  private slots:
    void on_instrument_changed(int index);
    void on_calculate();
    void on_calculate_secondary();

  private:
    // UI setup
    void setup_ui();
    QWidget* create_header_bar();
    QWidget* create_instrument_bar();
    QWidget* create_bonds_panel();
    QWidget* create_equity_options_panel();
    QWidget* create_fx_options_panel();
    QWidget* create_swaps_panel();
    QWidget* create_credit_panel();
    QWidget* create_status_bar();
    QWidget* create_results_panel();

    // Helpers
    QWidget* create_input_row(const QString& label, QWidget* input);
    QWidget* create_two_col(QWidget* left, QWidget* right);
    QWidget* create_three_col(QWidget* a, QWidget* b, QWidget* c);
    QDoubleSpinBox* create_spin(double val, double min_val, double max_val, int decimals, const QString& suffix = "");
    QDateEdit* create_date(const QDate& date);
    QComboBox* create_combo(const QStringList& items);
    QPushButton* create_calc_button(const QString& text);
    void display_results(const QJsonObject& result);
    void display_error(const QString& error);
    void clear_results();

    // Python bridge
    void run_pricing(const QString& command, const QStringList& args);

    // State
    int active_instrument_ = 0;  // 0=bonds, 1=equity, 2=fx, 3=swaps, 4=credit
    bool loading_ = false;

    // Layout
    QStackedWidget* panel_stack_ = nullptr;
    QWidget* results_container_ = nullptr;
    QLabel* status_instrument_ = nullptr;
    QLabel* status_engine_ = nullptr;

    // Instrument tab buttons
    QList<QPushButton*> instrument_btns_;

    // Bond inputs
    QDateEdit* bond_issue_date_ = nullptr;
    QDateEdit* bond_settle_date_ = nullptr;
    QDateEdit* bond_maturity_date_ = nullptr;
    QDoubleSpinBox* bond_coupon_ = nullptr;
    QDoubleSpinBox* bond_ytm_ = nullptr;
    QComboBox* bond_freq_ = nullptr;
    QDoubleSpinBox* bond_clean_price_ = nullptr;

    // Equity option inputs
    QDoubleSpinBox* opt_strike_ = nullptr;
    QDoubleSpinBox* opt_spot_ = nullptr;
    QDoubleSpinBox* opt_vol_ = nullptr;
    QDoubleSpinBox* opt_rate_ = nullptr;
    QDoubleSpinBox* opt_div_ = nullptr;
    QComboBox* opt_type_ = nullptr;
    QDoubleSpinBox* opt_time_ = nullptr;
    QDoubleSpinBox* opt_market_price_ = nullptr;

    // FX option inputs
    QDoubleSpinBox* fx_strike_ = nullptr;
    QDoubleSpinBox* fx_spot_ = nullptr;
    QDoubleSpinBox* fx_vol_ = nullptr;
    QDoubleSpinBox* fx_dom_rate_ = nullptr;
    QDoubleSpinBox* fx_for_rate_ = nullptr;
    QComboBox* fx_type_ = nullptr;
    QDoubleSpinBox* fx_time_ = nullptr;
    QDoubleSpinBox* fx_notional_ = nullptr;

    // Swap inputs
    QDateEdit* swap_eff_date_ = nullptr;
    QDateEdit* swap_mat_date_ = nullptr;
    QDoubleSpinBox* swap_fixed_rate_ = nullptr;
    QComboBox* swap_freq_ = nullptr;
    QDoubleSpinBox* swap_discount_ = nullptr;
    QDoubleSpinBox* swap_notional_ = nullptr;

    // CDS inputs
    QDateEdit* cds_val_date_ = nullptr;
    QDateEdit* cds_mat_date_ = nullptr;
    QDoubleSpinBox* cds_recovery_ = nullptr;
    QDoubleSpinBox* cds_spread_ = nullptr;
    QDoubleSpinBox* cds_notional_ = nullptr;
};

} // namespace fincept::screens
