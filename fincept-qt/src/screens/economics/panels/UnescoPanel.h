// src/screens/economics/panels/UnescoPanel.h
// UNESCO UIS (Institute for Statistics) — Education, Science & Tech, Culture data.
// No API key required. 200+ countries, 100,000+ indicators.
// Best command: fetch <indicator> <country> → [{date, value}]
// Script: unesco_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>

namespace fincept::screens {

class UnescoPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit UnescoPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    void load_indicators();
    void on_theme_changed(int index);
    void on_indicator_filter(const QString& text);

    QComboBox*   theme_combo_      = nullptr;
    QLineEdit*   indicator_search_ = nullptr;
    QListWidget* indicator_list_   = nullptr;
    QLineEdit*   country_input_    = nullptr;
    QLineEdit*   start_input_      = nullptr;
    QLineEdit*   end_input_        = nullptr;

    // preset indicators per theme
    struct IndicatorDef { QString name; QString code; };
    QList<IndicatorDef> current_indicators_;
    bool indicators_loaded_ = false;
};

} // namespace fincept::screens
