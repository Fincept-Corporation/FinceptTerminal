// src/screens/equity_research/EquityOwnershipTab.h
#pragma once
#include "screens/equity_research/EquityDataTab.h"
#include "services/fincept_data/FinceptDataModels.h"

#include <QVector>

class QGridLayout;

namespace fincept::ui {
class BarChart;
class DonutChart;
} // namespace fincept::ui

namespace fincept::screens {

/// OWNERSHIP — who holds the research symbol, from the Fincept /data API
/// (GET /data/ownership/{symbol}, one call): the ownership breakdown and net insider
/// activity, top institutional and fund holders, insider transactions and the
/// insider roster.
class EquityOwnershipTab : public EquityDataTab {
    Q_OBJECT
  public:
    explicit EquityOwnershipTab(QWidget* parent = nullptr);

  protected:
    void clear_view() override;
    void request(bool refresh) override;
    QString loading_text() const override;
    QString not_found_text(const services::fincept_data::DataError& e) const override;
    void retranslate() override;
    bool apply_remembered(const services::fincept_data::DataResult& r) override;

  private:
    struct Stat {
        QLabel* caption = nullptr;
        QLabel* value = nullptr;
    };
    void build_ui();
    Stat make_stat(QWidget* parent, QGridLayout* grid, int row, int column);
    void populate();
    void populate_stats();
    void populate_holders(QTableWidget* table, const QVector<services::fincept_data::Holder>& holders);
    void populate_transactions();
    void populate_roster();

    QVector<Stat> stats_;
    QLabel* sec_breakdown_ = nullptr;
    QLabel* sec_institutions_ = nullptr;
    QLabel* sec_funds_ = nullptr;
    QLabel* sec_transactions_ = nullptr;
    QLabel* sec_roster_ = nullptr;
    QTableWidget* institutions_table_ = nullptr;
    QTableWidget* funds_table_ = nullptr;
    QTableWidget* transactions_table_ = nullptr;
    QTableWidget* roster_table_ = nullptr;
    ui::DonutChart* holder_donut_ = nullptr; ///< insiders / institutions / public
    ui::BarChart* insider_chart_ = nullptr;  ///< net insider shares by month
    QLabel* holders_note_ = nullptr;
    QLabel* transactions_note_ = nullptr;

    services::fincept_data::OwnershipOverview data_;
    bool loaded_ = false;
};

} // namespace fincept::screens
