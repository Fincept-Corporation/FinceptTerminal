// src/screens/economics/panels/AdbPanel.h
// ADB (Asian Development Bank) — SDMX/KIDB API, no API key required.
// 14 Asia-Pacific economies, 7 dataflow categories.
// Script: adb_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLineEdit>

namespace fincept::screens {

class AdbPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit AdbPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    void on_category_changed(int index);

    QComboBox* economy_combo_   = nullptr;
    QComboBox* category_combo_  = nullptr;
    QLineEdit* indicator_input_ = nullptr;
    QLineEdit* start_input_     = nullptr;
    QLineEdit* end_input_       = nullptr;
};

} // namespace fincept::screens
