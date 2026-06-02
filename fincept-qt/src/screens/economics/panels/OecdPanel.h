// src/screens/economics/panels/OecdPanel.h
// OECD Statistics — GDP, CPI, unemployment, trade, interest rates.
// Script: oecd_data.py  (no API key, SDMX endpoints)
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>

namespace fincept::screens {

class OecdPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit OecdPanel(QWidget* parent = nullptr);
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
    QComboBox* frequency_combo_ = nullptr;

    // Cached for retranslateUi
    QLabel* dataset_lbl_ = nullptr;
    QLabel* country_lbl_ = nullptr;
    QLabel* freq_lbl_ = nullptr;
};

} // namespace fincept::screens
