// src/screens/economics/panels/FredAnalyticsPanel.h
// FRED Analytics — yield curve, money supply, credit, stress, sentiment, PCE.
// Script: fred_economic_data.py
// Requires FRED_API_KEY environment variable (same key as FredPanel).
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>

namespace fincept::screens {

class FredAnalyticsPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit FredAnalyticsPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    QComboBox* dataset_combo_ = nullptr;
};

} // namespace fincept::screens
