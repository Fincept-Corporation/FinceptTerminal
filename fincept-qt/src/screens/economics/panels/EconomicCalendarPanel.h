// src/screens/economics/panels/EconomicCalendarPanel.h
// Forex Factory economic calendar scraper — no API key required.
// Script: economic_calendar.py <mmmDD.YYYY>
// Returns: {date, url, events_count, events:[{time,currency,impact,event,actual,forecast,previous}]}
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QDateEdit>

namespace fincept::screens {

class EconomicCalendarPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit EconomicCalendarPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

  private:
    QDateEdit* date_edit_ = nullptr;
    QComboBox* filter_combo_ = nullptr; // filter by impact level
};

} // namespace fincept::screens
