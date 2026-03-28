// src/screens/economics/panels/OnsPanel.h
// UK Office for National Statistics panel.
// Script: ons_data.py  |  No API key required.
#pragma once

#include "screens/economics/panels/EconPanelBase.h"
#include <QComboBox>

namespace fincept::screens {

class OnsPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit OnsPanel(QWidget* parent = nullptr);
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
