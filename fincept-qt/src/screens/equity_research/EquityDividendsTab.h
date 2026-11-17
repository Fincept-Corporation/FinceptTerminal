// src/screens/equity_research/EquityDividendsTab.h
//
// Equity Research → Dividends: dividend history as annual bars, the trailing yield over
// time against its own average, payout on earnings and on free cash flow, growth CAGRs,
// the growth streak, and the next ex-dividend / pay dates. Amounts per share are in the
// quote's units as Yahoo reports them (pence for London), like the price they yield on.
#pragma once

#include "services/equity/EquityResearchModels.h"

#include <QLabel>
#include <QWidget>

class QTableWidget;

namespace fincept::ui {
class BarChart;
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::services::equity {
struct LoadFailure;
}

namespace fincept::screens {

namespace er_ui {
class StatusStrip;
}

class EquityDividendsTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityDividendsTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    void request(bool force);
    void render();

    QString current_symbol_;
    services::equity::EquityEvents events_;
    services::equity::FinancialsData fin_;
    QVector<services::equity::Candle> daily_;
    bool events_loaded_ = false;

    er_ui::StatusStrip* status_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QLabel* bars_title_ = nullptr;
    QLabel* yield_title_ = nullptr;
    QLabel* table_title_ = nullptr;
    QLabel* note_ = nullptr;
    ui::BarChart* bars_ = nullptr;
    ui::TimeSeriesChart* yield_chart_ = nullptr;
    QTableWidget* table_ = nullptr;
};

} // namespace fincept::screens
