// src/screens/economics/panels/FredPanel.h
// FRED (Federal Reserve Economic Data) — requires FRED_API_KEY env var.
// Script: fred_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLineEdit>

namespace fincept::screens {

class FredPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit FredPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    QLineEdit* series_input_ = nullptr;
    QComboBox* preset_combo_ = nullptr;
};

} // namespace fincept::screens
