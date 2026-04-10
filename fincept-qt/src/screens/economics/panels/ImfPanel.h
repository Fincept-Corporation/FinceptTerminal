// src/screens/economics/panels/ImfPanel.h
// IMF DataMapper — 133 macro indicators, all countries, pivot data.
// Script: imf_datamapper_data.py  (no API key required)
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>

namespace fincept::screens {

class ImfPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit ImfPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    void on_indicator_filter(const QString& text);
    // Flatten nested pivot: values.INDICATOR.COUNTRY.YEAR -> rows
    QJsonArray flatten_pivot(const QJsonObject& values, const QString& indicator_code,
                             const QString& country_filter) const;

    QLineEdit* indicator_search_ = nullptr;
    QListWidget* indicator_list_ = nullptr;
    QComboBox* country_combo_ = nullptr;
};

} // namespace fincept::screens
