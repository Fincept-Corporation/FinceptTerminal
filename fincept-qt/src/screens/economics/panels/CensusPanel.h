// src/screens/economics/panels/CensusPanel.h
// US Census Bureau — ACS demographic and housing data by state.
// Script: census_data.py
// Working commands: acs (population), housing
#pragma once

#include "screens/economics/panels/EconPanelBase.h"
#include <QComboBox>

namespace fincept::screens {

class CensusPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit CensusPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    /// Flatten Census {headers, data} shape into [{col1, col2, ...}] for display().
    static QJsonArray flatten_census(const QJsonObject& response);

    QComboBox* dataset_combo_ = nullptr;
};

} // namespace fincept::screens
