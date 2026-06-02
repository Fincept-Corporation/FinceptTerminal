// src/screens/economics/panels/TradingEconomicsPanel.h
// Trading Economics — bonds, yield curves, country data.
// Script: trading_economics_data.py
// Requires TRADING_ECONOMICS_API_KEY environment variable.
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>

namespace fincept::screens {

class TradingEconomicsPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit TradingEconomicsPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi() override;

    QComboBox* dataset_combo_ = nullptr;
    QComboBox* country_combo_ = nullptr;

    // Cached for retranslateUi
    QLabel* dataset_lbl_ = nullptr;
    QLabel* country_lbl_ = nullptr;
};

} // namespace fincept::screens
