// src/screens/equity_research/EquityEarningsTab.h
//
// Equity Research → Earnings: estimate vs actual EPS by fiscal quarter, the surprise, the
// next report date with its estimate range, and the share-price reaction around each
// report (−1/+1 and −5/+5 trading days, from the daily closes). Report dates Yahoo no
// longer serves are estimated from the trading volume and flagged as such. EPS is in the
// REPORTING currency, the reaction in percent.
#pragma once

#include "services/equity/EquityResearchModels.h"

#include <QLabel>
#include <QWidget>

class QTableWidget;

namespace fincept::ui {
class BarChart;
class KpiStrip;
} // namespace fincept::ui

namespace fincept::screens {

namespace er_ui {
class StatusStrip;
}

class EquityEarningsTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityEarningsTab(QWidget* parent = nullptr);
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
    QVector<services::equity::Candle> daily_;
    bool events_loaded_ = false;

    er_ui::StatusStrip* status_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QLabel* eps_title_ = nullptr;
    QLabel* surprise_title_ = nullptr;
    QLabel* table_title_ = nullptr;
    QLabel* note_ = nullptr;
    ui::BarChart* eps_chart_ = nullptr;
    ui::BarChart* surprise_chart_ = nullptr;
    QTableWidget* table_ = nullptr;
};

} // namespace fincept::screens
