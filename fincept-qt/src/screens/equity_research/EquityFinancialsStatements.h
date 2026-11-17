// src/screens/equity_research/EquityFinancialsStatements.h
//
// Financials → INCOME STATEMENT / BALANCE SHEET / CASH FLOW: a dense hierarchical table
// (model/view; sections and parent lines collapse; totals bold; indentation by level) with the
// period columns, a trend sparkline, YoY and CAGR beside them; the units switch re-expresses the
// cells (as reported in the chosen scale / common size / YoY %). Selecting a row plots that line
// big beside the table with its share of revenue (of total assets for the balance sheet) and its
// growth. Rows can be filtered and pinned to the top for comparison (right-click, or P).
#pragma once

#include "screens/equity_research/EquityFinancialsCommon.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;

namespace fincept::screens {

namespace fin_charts {
class FinComboChart;
}

class FinStatementTableModel;

class FinStatementPage : public QWidget {
    Q_OBJECT
  public:
    /// `statement`: 0 income, 1 balance sheet, 2 cash flow.
    explicit FinStatementPage(int statement, QWidget* parent = nullptr);

    void set_data(const FinBundlePtr& bundle, const FinViewState& vs);
    /// Highlight the column of analysis period `index` (matched by date).
    void set_selected(int index);
    /// The table as shown (all rows, nothing filtered or collapsed): plain numbers in full units
    /// (or fractions for common size / YoY) with an ISO currency column.
    void export_rows(QStringList& headers, QVector<QStringList>& rows) const;

  signals:
    void period_selected(int index);

  protected:
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override;

  private:
    void build_ui();
    void retranslate();
    void fit_columns();
    void show_line(int view_row);
    int column_for(int analysis_index) const;
    int analysis_for(int column) const;
    void context_menu(const QPoint& pos);

    int statement_ = 0;
    FinBundlePtr bundle_;
    FinViewState vs_;
    FinStatementTableModel* model_ = nullptr;
    QTableView* view_ = nullptr;
    QLineEdit* filter_ = nullptr;
    QPushButton* expand_btn_ = nullptr;
    QLabel* hint_ = nullptr;
    FinPanel table_panel_;
    FinPanel line_panel_;
    fin_charts::FinComboChart* line_chart_ = nullptr;
    QLabel* line_stats_ = nullptr;
    int current_src_row_ = -1;
    bool all_collapsed_ = false;
};

} // namespace fincept::screens
