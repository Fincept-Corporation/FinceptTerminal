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
    QVariantMap save_panel_state() const override;
    void restore_panel_state(const QVariantMap& state) override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi() override;

    QLineEdit* series_input_ = nullptr;
    QComboBox* preset_combo_ = nullptr;

    // Cached for retranslateUi
    QLabel* preset_lbl_ = nullptr;
    QLabel* series_lbl_ = nullptr;
};

} // namespace fincept::screens
