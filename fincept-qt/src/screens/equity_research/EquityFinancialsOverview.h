// src/screens/equity_research/EquityFinancialsOverview.h
//
// Financials → OVERVIEW: a grid of purposeful charts, each answering one question, re-flowing
// 1 / 2 / 3 columns with the width. Industrial companies: revenue & earnings, income waterfall,
// margin trend, balance-sheet structure, cash flow, cash bridge, returns (DuPont + ROIC vs a
// hurdle), growth heat map, working capital, capital return. Banks / insurers get their own set
// (net interest income, efficiency, capital strength) and a one-line reason where a chart does
// not apply. Clicking a period on any chart selects it everywhere (period_selected).
#pragma once

#include "screens/equity_research/EquityFinancialsCommon.h"

#include <QWidget>

class QGridLayout;
class QLabel;
class QScrollArea;

namespace fincept::ui {
class MatrixHeatmap;
class Sparkline;
} // namespace fincept::ui

namespace fincept::screens {

namespace fin_charts {
class FinComboChart;
class FinWaterfallChart;
class FinStackChart;
class FinDupontView;
} // namespace fin_charts

class FinOverviewPage : public QWidget {
    Q_OBJECT
  public:
    explicit FinOverviewPage(QWidget* parent = nullptr);

    /// Fill every chart from the bundle for the view state (basis, selected period, currency).
    void set_data(const FinBundlePtr& bundle, const FinViewState& vs);
    /// Only the selected period changed: move the selection bands, refill the period charts.
    void set_selected(int index);

  signals:
    void period_selected(int index);

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    struct Cell {
        QString key;
        FinPanel panel;
        QLabel* caption = nullptr;
    };

    void build_ui();
    void retranslate();
    Cell& add_cell(const QString& key);
    void relayout(bool force = false);
    void fill_trends();
    void fill_selected();
    QString period_label(int i) const;

    FinBundlePtr bundle_;
    FinViewState vs_;

    QScrollArea* scroll_ = nullptr;
    QWidget* body_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QVector<Cell> cells_;
    int columns_ = 0;
    QStringList shown_keys_; ///< cells shown for the current template, in grid order

    fin_charts::FinComboChart* rev_chart_ = nullptr;
    fin_charts::FinWaterfallChart* wf_chart_ = nullptr;
    fin_charts::FinComboChart* margin_chart_ = nullptr;
    fin_charts::FinStackChart* struct_chart_ = nullptr;
    fin_charts::FinComboChart* cash_chart_ = nullptr;
    fin_charts::FinWaterfallChart* bridge_chart_ = nullptr;
    fin_charts::FinDupontView* dupont_ = nullptr;
    fin_charts::FinComboChart* roic_chart_ = nullptr;
    ui::MatrixHeatmap* growth_ = nullptr;
    fin_charts::FinComboChart* wc_chart_ = nullptr;
    fin_charts::FinComboChart* capret_chart_ = nullptr;
    ui::Sparkline* shares_spark_ = nullptr;
    QLabel* shares_label_ = nullptr;
    QLabel* bank_note_ = nullptr;
};

} // namespace fincept::screens
