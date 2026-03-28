// src/screens/economics/panels/BlsPanel.h
// US Bureau of Labor Statistics — requires BLS_API_KEY env var.
// Script: bls_data.py
// Commands: get_popular, get_labor_overview, get_inflation_overview,
//           get_employment_cost_index, get_productivity_costs, get_series
#pragma once

#include "screens/economics/panels/EconPanelBase.h"
#include <QComboBox>
#include <QLineEdit>

namespace fincept::screens {

class BlsPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit BlsPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    QComboBox* preset_combo_  = nullptr;
    QLineEdit* series_input_  = nullptr;
};

} // namespace fincept::screens
