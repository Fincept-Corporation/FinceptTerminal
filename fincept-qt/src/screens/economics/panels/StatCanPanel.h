// src/screens/economics/panels/StatCanPanel.h
// Statistics Canada panel — GDP, CPI, unemployment, employment, population, housing.
// Script: statcan_data.py  |  No API key required.
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>

namespace fincept::screens {

class StatCanPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit StatCanPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    QComboBox* series_combo_ = nullptr;
};

} // namespace fincept::screens
