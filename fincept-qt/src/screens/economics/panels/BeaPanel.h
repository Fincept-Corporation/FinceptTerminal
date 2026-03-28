// src/screens/economics/panels/BeaPanel.h
// BEA (Bureau of Economic Analysis) — US National/Regional/Industry economic data.
// Requires BEA_API_KEY env var. Free registration at bea.gov/data/api/register
// Best command: fetch <indicator_id> <start_year> <end_year>
// Response: { "success": true, "data": [{"date": "YYYY", "value": 123.4}],
//             "metadata": {"indicator_name": "...", "source": "BEA NIPA"} }
// Script: bea_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>

namespace fincept::screens {

class BeaPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit BeaPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    void on_category_changed(int index);
    void on_indicator_filter(const QString& text);

    // Left sidebar
    QComboBox*   category_combo_    = nullptr;
    QLineEdit*   indicator_search_  = nullptr;
    QListWidget* indicator_list_    = nullptr;

    // Toolbar controls
    QLineEdit*   start_input_       = nullptr;
    QLineEdit*   end_input_         = nullptr;
    QComboBox*   freq_combo_        = nullptr;

    struct IndicatorDef { QString name; QString id; QString unit; };
    QList<IndicatorDef> current_indicators_;
};

} // namespace fincept::screens
