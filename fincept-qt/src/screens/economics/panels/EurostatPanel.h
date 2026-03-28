// src/screens/economics/panels/EurostatPanel.h
// Eurostat — EU statistical data (industrial, retail, energy, trade, etc.)
// Script: eurostat_extra_data.py
// Response: Eurostat SDMX-JSON with indexed value dict + dimension/time labels
#pragma once

#include "screens/economics/panels/EconPanelBase.h"
#include <QComboBox>

namespace fincept::screens {

class EurostatPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit EurostatPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    /// Flatten Eurostat SDMX-JSON indexed response into [{period, value}] rows.
    static QJsonArray flatten_sdmx(const QJsonObject& response);

    QComboBox* dataset_combo_ = nullptr;
    QComboBox* country_combo_ = nullptr;
};

} // namespace fincept::screens
