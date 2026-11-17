// src/screens/equity_research/EquityAnalystTab.h
#pragma once
#include "screens/equity_research/EquityDataTab.h"
#include "services/fincept_data/FinceptDataModels.h"

#include <QVector>

class QGridLayout;

namespace fincept::ui {
class BarChart;
class RangeBar;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

/// ANALYST — sell-side coverage of the research symbol from the Fincept /data API
/// (GET /data/analyst/{symbol}, one call): price targets, the recommendation trend,
/// EPS / revenue estimates with the EPS trend and revisions, earnings surprises and
/// the latest upgrades / downgrades.
class EquityAnalystTab : public EquityDataTab {
    Q_OBJECT
  public:
    explicit EquityAnalystTab(QWidget* parent = nullptr);

  protected:
    void clear_view() override;
    void request(bool refresh) override;
    QString loading_text() const override;
    QString not_found_text(const services::fincept_data::DataError& e) const override;
    void retranslate() override;
    bool apply_remembered(const services::fincept_data::DataResult& r) override;

  private:
    void build_ui();
    void populate();
    void populate_targets();
    void populate_trend();
    void populate_estimates();
    void populate_revisions();
    void populate_history();
    void populate_ratings();
    QString period_text(const QString& period) const;

    struct Stat {
        QLabel* caption = nullptr;
        QLabel* value = nullptr;
    };
    Stat make_stat(QWidget* parent, QGridLayout* grid, int column);

    QVector<Stat> target_stats_;
    QLabel* sec_targets_ = nullptr;
    QLabel* sec_trend_ = nullptr;
    QLabel* sec_estimates_ = nullptr;
    QLabel* sec_revisions_ = nullptr;
    QLabel* sec_history_ = nullptr;
    QLabel* sec_ratings_ = nullptr;
    QTableWidget* trend_table_ = nullptr;
    QTableWidget* estimates_table_ = nullptr;
    QTableWidget* revisions_table_ = nullptr;
    QTableWidget* history_table_ = nullptr;
    QTableWidget* ratings_table_ = nullptr;
    QLabel* ratings_note_ = nullptr;
    ui::RangeBar* target_range_ = nullptr;           ///< price vs the target range
    ui::BarChart* trend_chart_ = nullptr;            ///< recommendation mix per month (stacked)
    ui::TimeSeriesChart* revisions_chart_ = nullptr; ///< EPS estimate over the last 90 days
    ui::BarChart* history_chart_ = nullptr;          ///< estimate vs actual EPS

    services::fincept_data::AnalystOverview data_;
    bool loaded_ = false;
};

} // namespace fincept::screens
