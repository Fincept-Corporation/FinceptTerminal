// src/screens/economics/panels/EconDbPanel.h
// EconDB — macroeconomic indicators for 49 countries, no API key required.
// Script: econdb_data.py
// Command: indicator <code> <country_iso2>
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>

namespace fincept::screens {

class EconDbPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit EconDbPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    QComboBox* indicator_combo_ = nullptr;
    QComboBox* country_combo_ = nullptr;
};

} // namespace fincept::screens
