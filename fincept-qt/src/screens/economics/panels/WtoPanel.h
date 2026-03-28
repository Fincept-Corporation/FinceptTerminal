// src/screens/economics/panels/WtoPanel.h
// WTO (World Trade Organization) — Timeseries, QR, and TFAD APIs.
// Requires WTO_API_KEY (Ocp-Apim-Subscription-Key) for timeseries data.
// QR members endpoint is free.
// Script: wto_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>

namespace fincept::screens {

class WtoPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit WtoPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    void on_section_changed(int index);

    QComboBox* section_combo_    = nullptr;  // Trade Statistics | QR Members | QR Notifications
    QComboBox* indicator_combo_  = nullptr;  // timeseries indicators
    QLineEdit* reporter_input_   = nullptr;  // reporter member code e.g. US, CN
    QLineEdit* years_input_      = nullptr;  // period e.g. 2015-2023
    QLabel*    apikey_notice_    = nullptr;
};

} // namespace fincept::screens
