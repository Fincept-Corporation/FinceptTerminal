// src/screens/economics/panels/WorldBankPanel.h
// World Bank Open Data — 296 countries, 70+ sources, 1000s of indicators.
// Script: worldbank_data.py  (no API key required)
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>

namespace fincept::screens {

class WorldBankPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit WorldBankPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    void load_countries();
    void on_country_filter(const QString& text);
    void on_indicator_filter(const QString& text);

    QLineEdit*   country_search_    = nullptr;
    QListWidget* country_list_      = nullptr;
    QLineEdit*   indicator_search_  = nullptr;
    QListWidget* indicator_list_    = nullptr;
    QComboBox*   date_preset_       = nullptr;

    QString selected_country_    = "US";
    QString selected_indicator_;
    bool    countries_loaded_    = false;
};

} // namespace fincept::screens
