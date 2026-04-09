// src/screens/economics/panels/AkShareChinaPanel.h
// AkShare China macroeconomic data — no API key required.
// Script: akshare_economics_china.py
// Working commands: cpi, ppi, gdp, pmi
// Note: column names are in Chinese — displayed as-is in the table.
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>

namespace fincept::screens {

class AkShareChinaPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit AkShareChinaPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    QComboBox* series_combo_ = nullptr;
};

} // namespace fincept::screens
