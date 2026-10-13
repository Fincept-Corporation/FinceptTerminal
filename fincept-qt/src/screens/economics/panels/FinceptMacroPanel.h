// src/screens/economics/panels/FinceptMacroPanel.h
// Fincept Macro — Fincept's own macro API (api.fincept.in /macro/*), billed in Fincept credits:
//   WorldGovernmentBonds snapshots: central-bank rates, sovereign CDS, 10Y yields & spreads, credit
//   ratings, inverted curves, yield forecasts, per-country bond summary;
//   the upcoming macro-event calendar; CEIC time series (explorer) and the CEIC indicator catalogue.
// All I/O goes through FinceptMacroService (cached; stale copies served when offline). This panel
// only renders, maps 401 / 402 / offline to actionable states, and shows the credit cost up front.
#pragma once

#include "screens/economics/panels/EconPanelBase.h"
#include "services/economics/FinceptMacroService.h"

#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

namespace fincept::screens {

class FinceptMacroPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit FinceptMacroPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    /// Unused: this panel's data comes from FinceptMacroService, not EconomicsService.
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi() override;
    QString current_view() const;
    void update_controls_for_view();
    void update_credit_hint();
    void run_query(bool force);
    void load_ceic_countries();
    void load_ceic_indicators();
    void on_macro_result(const services::FinceptMacroResult& result);
    void show_failure(const services::FinceptMacroResult& result);
    void show_view_result(const services::FinceptMacroResult& result);

    QLabel* view_lbl_ = nullptr;
    QComboBox* view_combo_ = nullptr;
    QLabel* country_lbl_ = nullptr;
    QLineEdit* country_edit_ = nullptr;
    QLabel* maturity_lbl_ = nullptr;
    QComboBox* maturity_combo_ = nullptr;
    QLabel* importance_lbl_ = nullptr;
    QComboBox* importance_combo_ = nullptr;
    QLabel* days_lbl_ = nullptr;
    QSpinBox* days_spin_ = nullptr;
    QLabel* ceic_country_lbl_ = nullptr;
    QComboBox* ceic_country_combo_ = nullptr;
    QLabel* ceic_indicator_lbl_ = nullptr;
    QComboBox* ceic_indicator_combo_ = nullptr;
    QLabel* years_lbl_ = nullptr;
    QSpinBox* year_from_spin_ = nullptr;
    QSpinBox* year_to_spin_ = nullptr;
    QLabel* category_lbl_ = nullptr;
    QLineEdit* category_edit_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* credit_lbl_ = nullptr;

    QString pending_request_id_; // the data request whose answer the panel is waiting for
    QString pending_view_;       // view that request belongs to
    int request_seq_ = 0;
    bool ceic_countries_requested_ = false;
    QString ceic_indicators_country_; // country whose indicator list is loaded / loading
};

} // namespace fincept::screens
