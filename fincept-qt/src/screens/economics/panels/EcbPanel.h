// src/screens/economics/panels/EcbPanel.h
// European Central Bank — exchange rates, inflation, money supply.
// Script: ecb_sdmx_data.py
// Working commands: exchange_rates, inflation, money_supply
#pragma once

#include "screens/economics/panels/EconPanelBase.h"
#include <QComboBox>

namespace fincept::screens {

class EcbPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit EcbPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    QComboBox* series_combo_  = nullptr;
    QComboBox* country_combo_ = nullptr;  // for exchange_rates currency
};

} // namespace fincept::screens
