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
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;
    void changeEvent(QEvent* event) override;

  private:
    void on_category_changed(int index);
    void retranslateUi() override;

    QComboBox* economy_combo_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QLineEdit* indicator_input_ = nullptr;
    QLineEdit* start_input_ = nullptr;
    QLineEdit* end_input_ = nullptr;

    // Cached for retranslateUi
    QLabel* economy_lbl_ = nullptr;
    QLabel* category_lbl_ = nullptr;
    QLabel* indicator_lbl_ = nullptr;
    QLabel* from_lbl_ = nullptr;
    QLabel* to_lbl_ = nullptr;
};

} // namespace fincept::screens
