// src/screens/economics/panels/WorldBankHealthPanel.h
// World Bank Health & Development indicators.
// Script: worldbank_health_data.py
// Commands: life_expectancy, infant_mortality, literacy, gini, hdi, poverty
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>

namespace fincept::screens {

class WorldBankHealthPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit WorldBankHealthPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    /// Flatten WB records [{date, value, indicator:{value}, country:{value}}]
    /// into [{date, value}], filtering null values, sorted oldest→newest.
    static QJsonArray flatten_wb(const QJsonObject& response);

    QComboBox* indicator_combo_ = nullptr;
    QComboBox* country_combo_ = nullptr;
};

} // namespace fincept::screens
