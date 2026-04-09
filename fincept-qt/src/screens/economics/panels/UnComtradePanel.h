// src/screens/economics/panels/UnComtradePanel.h
// UN Comtrade — international trade statistics (free tier).
// Script: un_comtrade_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLineEdit>

namespace fincept::screens {

class UnComtradePanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit UnComtradePanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    QComboBox* reporter_combo_ = nullptr;
    QComboBox* flow_combo_ = nullptr;
    QComboBox* period_combo_ = nullptr;
    QComboBox* cmd_combo_ = nullptr;
};

} // namespace fincept::screens
