// src/screens/economics/panels/IlostatPanel.h
// ILO ILOSTAT — unemployment, labour force participation, employment-to-population.
// Script: ilostat_data.py  |  No API key required.
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLineEdit>

namespace fincept::screens {

class IlostatPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit IlostatPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    QComboBox* series_combo_ = nullptr;
    QLineEdit* country_edit_ = nullptr;
    QLineEdit* start_edit_ = nullptr;
    QLineEdit* end_edit_ = nullptr;
};

} // namespace fincept::screens
