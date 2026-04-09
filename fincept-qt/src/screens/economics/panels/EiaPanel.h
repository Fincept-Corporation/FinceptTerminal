// src/screens/economics/panels/EiaPanel.h
// EIA (U.S. Energy Information Administration)
// WPSR (Weekly Petroleum Status Report): no API key — downloads public XLS files.
// STEO (Short-Term Energy Outlook): requires EIA_API_KEY env var.
// Script: eia_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>

namespace fincept::screens {

class EiaPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit EiaPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    void on_source_changed(int index);

    QComboBox* source_combo_ = nullptr;   // WPSR | STEO
    QComboBox* category_combo_ = nullptr; // WPSR categories or STEO tables
    QLabel* apikey_notice_ = nullptr;
};

} // namespace fincept::screens
