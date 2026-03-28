// src/screens/economics/panels/FiscalDataPanel.h
// US Treasury FiscalData panel — debt, interest rates, exchange rates.
// Script: fiscal_data.py  |  No API key required.
#pragma once

#include "screens/economics/panels/EconPanelBase.h"
#include <QComboBox>

namespace fincept::screens {

class FiscalDataPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit FiscalDataPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    QComboBox* series_combo_ = nullptr;
};

} // namespace fincept::screens
