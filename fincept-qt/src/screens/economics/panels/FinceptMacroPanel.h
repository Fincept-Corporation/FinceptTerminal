// src/screens/economics/panels/FinceptMacroPanel.h
// Fincept Macro — proprietary macro data source.
// Script fincept_macro.py does not yet exist — shows a Coming Soon panel.
// When the script is ready, this panel will be updated.
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

namespace fincept::screens {

class FinceptMacroPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit FinceptMacroPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;
};

} // namespace fincept::screens
