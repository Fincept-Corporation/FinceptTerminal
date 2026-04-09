// src/screens/economics/panels/BisPanel.h
// BIS (Bank for International Settlements) — SDMX API, no API key required.
// 13 statistical domains: exchange rates, central bank rates, credit, house prices, etc.
// Script: bis_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLineEdit>

namespace fincept::screens {

class BisPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit BisPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    void on_dataset_changed(int index);

    QComboBox* dataset_combo_ = nullptr;
    QLineEdit* country_input_ = nullptr;
    QLineEdit* start_input_ = nullptr;
    QLineEdit* end_input_ = nullptr;
};

} // namespace fincept::screens
