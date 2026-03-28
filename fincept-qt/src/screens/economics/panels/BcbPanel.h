// src/screens/economics/panels/BcbPanel.h
// Brazil Central Bank (BCB / Banco Central do Brasil) — no API key required.
// Script: bcb_data.py
// Working commands: selic, ipca, gdp, unemployment, credit, reserves
#pragma once

#include "screens/economics/panels/EconPanelBase.h"
#include <QComboBox>

namespace fincept::screens {

class BcbPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit BcbPanel(QWidget* parent = nullptr);
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
