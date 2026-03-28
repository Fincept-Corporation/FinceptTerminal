// src/screens/economics/panels/GlobalCentralBanksPanel.h
// Global Central Banks panel — BOE, RBA, BOC, Riksbank, SNB, Norges Bank.
// No API key required for any source.
#pragma once

#include "screens/economics/panels/EconPanelBase.h"
#include <QComboBox>

namespace fincept::screens {

class GlobalCentralBanksPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit GlobalCentralBanksPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    QComboBox* bank_combo_   = nullptr;
    QComboBox* series_combo_ = nullptr;

    void update_series_for_bank(int bank_idx);
};

} // namespace fincept::screens
